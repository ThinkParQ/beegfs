#ifndef TABLE_H_
#define TABLE_H_

#include <database/Buffer.h>
#include <database/Filter.h>
#include <database/LeftJoinEq.h>
#include <database/Select.h>
#include <database/Set.h>

#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/ref.hpp>
#include <boost/weak_ptr.hpp>

template<typename Data, bool AllowDuplicateKeys = false>
class Table {
   private:
      struct Key
      {
         typedef typename Data::KeyType KeyType;

         KeyType value;

         Key() {}

         Key(const KeyType& key) : value(key) {}

         KeyType pkey() const { return value; }

         bool operator<(const Key& r) const { return value < r.value; }
         bool operator==(const Key& r) const { return value == r.value; }
      };

      struct GetKey
      {
         typedef typename Data::KeyType result_type;

         result_type operator()(const Data& data) const { return data.pkey(); }
         result_type operator()(const Key& data) const { return data.value; }
      };

      struct SecondIsNull
      {
         typedef bool result_type;

         bool operator()(std::pair<Data, Key*>& p) const { return p.second == NULL; }
      };

      struct First
      {
         typedef Data result_type;

         Data operator()(std::pair<Data, Key*>& p) const { return p.first; }
      };

      struct HasMatchingKey
      {
         HasMatchingKey(const typename Data::KeyType& k) : key(k) {}
         typename Data::KeyType key;

         bool operator()(std::pair<Data, Key*>& p) const
         {
            return (p.first.pkey() == key);
         }
      };

   private:
      std::string basename;
      size_t fragmentSize;

      Set<Data> base;
      Set<Data> inserts;
      Set<Key> deletes;

      typename Data::KeyType lastChangeKey;
      Set<Data> insertsPending;
      Set<Key> deletesPending;

      boost::scoped_ptr<Buffer<Data> > insertBuffer;
      boost::scoped_ptr<Buffer<Key> > deleteBuffer;

      enum {
         ms_none,
         ms_ordered_only_insert,
         ms_ordered_only_delete,
         ms_ordered_at_insert,
         ms_ordered_at_delete,
         ms_unordered_insert,
         ms_unordered_delete,
         ms_bulk_insert,
      } modificationState;

      bool dropped;
      bool insertsIntoBase;

      std::vector<boost::weak_ptr<Buffer<Data> > > bulkInserters;

      void assertNoBulkInsert()
      {
         for(size_t i = 0; i < bulkInserters.size(); i++)
         {
            if(!bulkInserters[i].expired() )
               throw std::runtime_error("bulk insert still running");
         }

         bulkInserters.clear();
      }

      void collapseChanges()
      {
         deleteBuffer.reset();
         insertBuffer.reset();
         insertsIntoBase = false;

         if(deletesPending.size() )
         {
            Set<Data> insertsTemp(basename + ".it");

            LeftJoinEq<
                  SetFragmentCursor<Data>,
                  SetFragmentCursor<Key>,
                  GetKey>
               cleanedInserts = db::leftJoinBy(
                  GetKey(),
                  inserts.cursor(),
                  deletesPending.cursor() );

            // sequence will always be sorted, no need to fragment and sort again
            SetFragment<Data>* fragment = insertsTemp.newFragment();
            while(cleanedInserts.step() )
            {
               if (cleanedInserts.get()->second == nullptr)
                  fragment->append(cleanedInserts.get()->first);
            }

            inserts.clear();
            insertsTemp.mergeInto(inserts);
            insertsTemp.drop();

            deletesPending.mergeInto(deletes);
            deletes.makeUnique();
         }

         if(insertsPending.size() )
            insertsPending.mergeInto(inserts);

         modificationState = ms_none;
      }

      void setupBaseBuffer()
      {
         insertBuffer.reset(new Buffer<Data>(base, fragmentSize));
         insertsIntoBase = true;
      }

   public:
      Table(const std::string& basename, size_t fragmentSize, bool allowCreate = true)
         : basename(basename), fragmentSize(fragmentSize),
           base(basename + ".b", allowCreate),
           inserts(basename + ".i", allowCreate),
           deletes(basename + ".d", allowCreate),
           insertsPending(basename + ".ip", true),
           deletesPending(basename + ".dp", true),
           modificationState(ms_none),
           dropped(false), insertsIntoBase(false)
      {
         if (base.size() == 0)
            setupBaseBuffer();
      }

      ~Table()
      {
         if(dropped)
            return;

         collapseChanges();

         deletesPending.drop();
         insertsPending.drop();
      }

      boost::shared_ptr<Buffer<Data> > bulkInsert()
      {
         if(inserts.size() || deletes.size() || insertsPending.size() || deletesPending.size() )
            throw std::runtime_error("unordered operation");

         switch(modificationState)
         {
         case ms_none:
         case ms_ordered_only_insert:
         case ms_bulk_insert:
            break;

         default:
            throw std::runtime_error("unordered operation");
         }

         modificationState = ms_bulk_insert;
         boost::shared_ptr<Buffer<Data> > buffer = boost::make_shared<Buffer<Data> >(
            boost::ref(base), fragmentSize);

         bulkInserters.push_back(buffer);
         return buffer;
      }

      void insert(const Data& data)
      {
         using namespace std::rel_ops;

         switch(modificationState)
         {
         case ms_bulk_insert:
            assertNoBulkInsert();
            BEEGFS_FALLTHROUGH;

         case ms_none:
            modificationState = ms_ordered_only_insert;
            break;

         case ms_ordered_only_insert:
            if(lastChangeKey >= data.pkey() )
               modificationState = ms_unordered_insert;

            break;

         case ms_unordered_delete:
            throw std::runtime_error("unordered operation");

         case ms_unordered_insert:
            break;

         case ms_ordered_at_delete:
         case ms_ordered_only_delete:
            if(lastChangeKey <= data.pkey() )
               modificationState = ms_ordered_at_insert;
            else
               throw std::runtime_error("unordered operation");

            break;

         case ms_ordered_at_insert:
            if(lastChangeKey < data.pkey()
                  || (!AllowDuplicateKeys && lastChangeKey == data.pkey() ) )
               throw std::runtime_error("unordered operation");

            break;
         }

         lastChangeKey = data.pkey();

         if(!insertBuffer)
            insertBuffer.reset(new Buffer<Data>(insertsPending, fragmentSize) );

         insertBuffer->append(data);
      }

      void remove(const typename Data::KeyType& key)
      {
         using namespace std::rel_ops;

         if(modificationState == ms_bulk_insert)
            assertNoBulkInsert();

         // if deletes happen during initial insert, complete the base set. delete tracking
         // cannot cope otherwise
         if(insertsIntoBase)
         {
            insertBuffer.reset();
            insertsIntoBase = false;
            modificationState = ms_none;
         }

         switch(modificationState)
         {
         case ms_none:
         case ms_bulk_insert:
            modificationState = ms_ordered_only_delete;
            break;

         case ms_ordered_only_delete:
            if(lastChangeKey >= key)
               modificationState = ms_unordered_delete;

            break;

         case ms_unordered_delete:
            break;

         case ms_unordered_insert:
            throw std::runtime_error("unordered operation");

         case ms_ordered_at_delete:
         case ms_ordered_at_insert:
         case ms_ordered_only_insert:
            if(key <= lastChangeKey)
               throw std::runtime_error("unordered operation");

            modificationState = ms_ordered_at_delete;
            break;
         }

         lastChangeKey = key;

         if(!deleteBuffer)
            deleteBuffer.reset(new Buffer<Key>(deletesPending, fragmentSize) );

         deleteBuffer->append(key);
      }

      void commitChanges()
      {
         collapseChanges();
         base.sort();
         inserts.sort();
         deletes.sort();
      }

      typedef Union<
         Select<
            Filter<
               LeftJoinEq<
                  SetFragmentCursor<Data>,
                  SetFragmentCursor<Key>,
                  GetKey>,
               SecondIsNull>,
            First>,
         SetFragmentCursor<Data>,
         GetKey> QueryType;

      QueryType cursor()
      {
         return db::unionBy(
            GetKey(),
            db::leftJoinBy(
               GetKey(),
               base.cursor(),
               deletes.cursor() )
            | db::where(SecondIsNull() )
            | db::select(First() ),
            inserts.cursor() );
      }

      typedef Union<
         Select<
            Filter<
               Filter<
                  LeftJoinEq<
                     SetFragmentCursor<Data>,
                     SetFragmentCursor<Key>,
                     GetKey>,
                  SecondIsNull>,
               HasMatchingKey>,
            First>,
         SetFragmentCursor<Data>,
         GetKey> MultiRowResultType;

      MultiRowResultType getAllByKey(const typename Data::KeyType& key)
      {
         return db::unionBy(
            GetKey(),
            db::leftJoinBy(
               GetKey(),
               base.cursor(),
               deletes.cursor() )
            | db::where(SecondIsNull() )
            | db::where(HasMatchingKey(key) )
            | db::select(First() ),
            inserts.cursor() );
      }

      template<typename Key, typename Fn>
      std::pair<bool, Data> getByKeyProjection(const Key& key, Fn fn)
      {
         Data dummy = {};

         std::pair<bool, Data> insIdx = inserts.getByKeyProjection(key, fn);
         if(insIdx.first)
            return insIdx;

         std::pair<bool, Data> baseIdx = base.getByKeyProjection(key, fn);
         if(!baseIdx.first)
            return std::make_pair(false, dummy);

         std::pair<bool, typename Table::Key> delIdx = deletes.getByKey(baseIdx.second.pkey() );
         if(delIdx.first)
            return std::make_pair(false, dummy);

         return baseIdx;
      }

      std::pair<bool, Data> getByKey(const typename Data::KeyType& key)
      {
         struct ops
         {
            static typename Data::KeyType key(const typename Data::KeyType& key) { return key; }
         };

         return getByKeyProjection(key, ops::key);
      }

      void drop()
      {
         insertBuffer.reset();
         deleteBuffer.reset();

         base.drop();
         inserts.drop();
         deletes.drop();
         insertsPending.drop();
         deletesPending.drop();
         dropped = true;
      }

      void clear()
      {
         insertBuffer.reset();
         deleteBuffer.reset();

         base.clear();
         inserts.clear();
         deletes.clear();
         insertsPending.clear();
         deletesPending.clear();

         setupBaseBuffer();
      }
};

#endif
