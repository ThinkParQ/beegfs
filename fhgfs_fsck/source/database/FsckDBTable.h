#ifndef FsckDBTable_h_drBbZVcUQetDB8UVnMd1Uj
#define FsckDBTable_h_drBbZVcUQetDB8UVnMd1Uj

#include <common/fsck/FsckModificationEvent.h>
#include <database/Chunk.h>
#include <database/ContDir.h>
#include <database/DirEntry.h>
#include <database/DirInode.h>
#include <database/FileInode.h>
#include <database/FsID.h>
#include <database/Group.h>
#include <database/Table.h>
#include <database/ModificationEvent.h>
#include <database/StripeTargets.h>
#include <database/UsedTarget.h>

#include <fstream>

class FsckDBDentryTable
{
   public:
      struct ByParent
      {
         db::EntryID parent; /* 12 */
         db::EntryID target; /* 24 */
         uint32_t entryType; /* 28 */

         typedef std::pair<db::EntryID, db::EntryID> KeyType;

         KeyType pkey() const { return KeyType(parent, target); }
      };

      class NameBuffer
      {
         public:
            NameBuffer(const std::string& path, unsigned id):
               fileID(id)
            {
               streamBuf.resize(4096 * 1024);

               wstream.rdbuf()->pubsetbuf(&streamBuf[0], streamBuf.size());
               wstream.exceptions(std::fstream::badbit | std::fstream::failbit);
               wstream.open(path.c_str(), std::fstream::out | std::fstream::app);
               wstream.seekp(0, std::fstream::end);
               writeOffset = wstream.tellp();

               rstream.exceptions(std::fstream::badbit | std::fstream::failbit);
               rstream.open(path.c_str(), std::fstream::in | std::fstream::app);
            }

            uint64_t put(const std::string& name)
            {
               unsigned len = name.size() + 1;
               this->wstream.write(name.c_str(), len);
               this->writeOffset += len;
               return this->writeOffset - len;
            }

            std::string get(uint64_t offset)
            {
               wstream.flush();
               rstream.clear();
               rstream.seekg(offset);

               char buffer[255 + 1];
               rstream.getline(buffer, sizeof(buffer), 0);
               return buffer;
            }

            unsigned id() const { return fileID; }

         private:
            unsigned fileID;
            std::vector<char> streamBuf;

            // the data stream is split into two parts because libstdc++ before version 4.6 has a
            // bug affects fstreams used for reading and writing, see:
            //   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=45708
            // while seeking to the required position before each read/write is supposed to have the
            // desired effect (ie, reads and writes work), this does not seem to be the case.
            std::ofstream wstream;
            uint64_t writeOffset;

            std::ifstream rstream;
      };

      struct BulkHandle
      {
         boost::shared_ptr<Buffer<db::DirEntry> > dentries;
         boost::shared_ptr<Buffer<ByParent> > byParent;
         NameBuffer* nameBuffer;
      };

   private:
      struct NameCacheEntry
      {
         db::EntryID id;
         db::EntryID parent;
         std::string name;

         std::list<NameCacheEntry*>::iterator lruLink;
      };

   public:
      FsckDBDentryTable(const std::string& dbPath, size_t fragmentSize, size_t nameCacheLimit,
            bool allowCreate)
         : dbPath(dbPath),
           table(dbPath + "/dentries", fragmentSize, allowCreate),
           byParent(dbPath + "/dentriesbyparent", fragmentSize, allowCreate),
           insertSeqNo(0),
           nameCacheLimit(nameCacheLimit)
      {
      }

      void insert(FsckDirEntryList& dentries, const BulkHandle* handle = NULL);
      void updateFieldsExceptParent(FsckDirEntryList& dentries);
      void remove(FsckDirEntryList& dentries);

      Table<db::DirEntry>::QueryType get();
      std::pair<bool, db::DirEntry> getAnyFor(db::EntryID id);

      Table<ByParent>::QueryType getByParent();

      std::string getNameOf(const db::DirEntry& dentry);
      std::string getPathOf(const db::DirEntry& dentry);

   private:
      std::string dbPath;
      Table<db::DirEntry> table;
      Table<ByParent> byParent;

      std::map<uint64_t, boost::shared_ptr<NameBuffer> > nameBuffers;
      uint64_t insertSeqNo;

      std::map<db::EntryID, NameCacheEntry> nameCache;
      std::list<NameCacheEntry*> nameCacheLRU;
      size_t nameCacheLimit;

      NameBuffer& getNameBuffer(unsigned id)
      {
         if(this->nameBuffers.count(id) )
            return *this->nameBuffers[id];

         const std::string& nextNameFile = dbPath + "/dentrynames."
            + StringTk::intToStr(id);

         boost::shared_ptr<NameBuffer> buf = boost::make_shared<NameBuffer>(
            nextNameFile, id);
         this->nameBuffers[id] = buf;
         return *buf;
      }

      void addToCache(const db::DirEntry& entry, const std::string& name)
      {
         if(nameCache.size() >= nameCacheLimit)
         {
            nameCache.erase(nameCacheLRU.front()->id);
            nameCacheLRU.pop_front();
         }

         NameCacheEntry cacheEntry = { entry.id, entry.parentDirID, name, nameCacheLRU.end() };
         std::pair<std::map<db::EntryID, NameCacheEntry>::iterator, bool> pos = nameCache.insert(
            std::make_pair(entry.id, cacheEntry) );

         if(!pos.second)
            return;

         pos.first->second.lruLink = nameCacheLRU.insert(nameCacheLRU.end(), &pos.first->second);
      }

      const NameCacheEntry* getFromCache(db::EntryID id)
      {
         std::map<db::EntryID, NameCacheEntry>::iterator pos = nameCache.find(id);
         if(pos == nameCache.end() )
            return NULL;

         nameCacheLRU.splice(nameCacheLRU.end(), nameCacheLRU, pos->second.lruLink);
         return &pos->second;
      }

   public:
      void clear()
      {
         this->table.clear();
         this->byParent.clear();
         this->insertSeqNo = 0;
         this->nameCache.clear();
         this->nameCacheLRU.clear();
      }

      BulkHandle newBulkHandle()
      {
         BulkHandle result = {
            this->table.bulkInsert(),
            this->byParent.bulkInsert(),
            &getNameBuffer(this->nameBuffers.size() ),
         };
         return result;
      }

      void flush(BulkHandle& handle)
      {
         handle = {{}, {}, nullptr};
      }

      void insert(FsckDirEntryList& dentries, const BulkHandle& handle)
      {
         insert(dentries, &handle);
      }

      std::string getNameOf(db::EntryID id)
      {
         if(id == db::EntryID::anchor() || id == db::EntryID::root() )
            return "";
         else
         if(id == db::EntryID::disposal() )
            return "[<disposal>]";
         else
         if(id == db::EntryID::mdisposal() )
            return "[<mdisposal>]";

         std::pair<bool, db::DirEntry> item = getAnyFor(id);
         if(!item.first)
            return "[<unresolved>]";
         else
            return getNameOf(item.second);
      }

      std::string getPathOf(db::EntryID id)
      {
         std::pair<bool, db::DirEntry> item = getAnyFor(id);
         if(!item.first)
            return "[<unresolved>]";
         else
            return getPathOf(item.second);
      }
};

class FsckDBFileInodesTable
{
   public:
      typedef std::pair<
         boost::shared_ptr<Buffer<db::FileInode> >,
         boost::shared_ptr<Buffer<db::StripeTargets> > > BulkHandle;

      struct UniqueInlinedInode
      {
         typedef boost::tuple<db::EntryID, uint64_t, uint32_t, int32_t> KeyType;
         typedef db::FileInode ProjType;
         typedef int GroupType;

         KeyType key(const db::FileInode& fi)
         {
            return boost::make_tuple(fi.id, fi.saveInode, fi.saveNodeID, fi.saveDevice);
         }

         db::FileInode project(const db::FileInode& fi)
         {
            return fi;
         }

         void step(const db::FileInode& fi) {}

         GroupType finish()
         {
            return 0;
         }
      };

      struct SelectFirst
      {
         typedef db::FileInode result_type;

         db::FileInode operator()(std::pair<db::FileInode, int>& pair) const
         {
            return pair.first;
         }
      };

      typedef
         Select<
            Group<
               Table<db::FileInode>::QueryType,
               UniqueInlinedInode>,
            SelectFirst> InodesQueryType;

   public:
      FsckDBFileInodesTable(const std::string& dbPath, size_t fragmentSize, bool allowCreate)
         : inodes(dbPath + "/fileinodes", fragmentSize, allowCreate),
           targets(dbPath + "/filetargets", fragmentSize, allowCreate)
      {
      }

      void insert(FsckFileInodeList& fileInodes, const BulkHandle* handle = NULL);
      void update(FsckFileInodeList& inodes);
      void remove(FsckFileInodeList& fileInodes);

      InodesQueryType getInodes();
      Table<db::StripeTargets, true>::QueryType getTargets();

      std::pair<bool, db::FileInode> get(std::string id);

   private:
      Table<db::FileInode> inodes;
      Table<db::StripeTargets, true> targets;

   public:
      void clear()
      {
         inodes.clear();
         targets.clear();
      }

      void remove(db::EntryID id)
      {
         inodes.remove(id);
         targets.remove(id);
      }

      BulkHandle newBulkHandle()
      {
         return BulkHandle(this->inodes.bulkInsert(), this->targets.bulkInsert() );
      }

      void flush(BulkHandle& handle)
      {
         handle = {};
      }

      void insert(FsckFileInodeList& fileInodes, const BulkHandle& handle)
      {
         insert(fileInodes, &handle);
      }
};

class FsckDBDirInodesTable
{
   public:
      typedef boost::shared_ptr<Buffer<db::DirInode> > BulkHandle;

   public:
      FsckDBDirInodesTable(const std::string& dbPath, size_t fragmentSize, bool allowCreate)
         : table(dbPath + "/dirinodes", fragmentSize, allowCreate)
      {
      }

      void insert(FsckDirInodeList& fileInodes, const BulkHandle* handle = NULL);
      void update(FsckDirInodeList& inodes);
      void remove(FsckDirInodeList& dirInodes);

      Table<db::DirInode>::QueryType get();
      std::pair<bool, FsckDirInode> get(std::string id);

   private:
      Table<db::DirInode> table;

   public:
      void clear()
      {
         this->table.clear();
      }

      BulkHandle newBulkHandle()
      {
         return this->table.bulkInsert();
      }

      void flush(BulkHandle& handle)
      {
         handle = {};
      }

      void insert(FsckDirInodeList& fileInodes, const BulkHandle& handle)
      {
         insert(fileInodes, &handle);
      }
};

class FsckDBChunksTable
{
   public:
      typedef boost::shared_ptr<Buffer<db::Chunk> > BulkHandle;

   public:
      FsckDBChunksTable(const std::string& dbPath, size_t fragmentSize, bool allowCreate)
         : table(dbPath + "/chunks", fragmentSize, allowCreate)
      {
      }

      void insert(FsckChunkList& chunks, const BulkHandle* handle = NULL);
      void update(FsckChunkList& chunks);
      void remove(const db::Chunk::KeyType& id);

      Table<db::Chunk>::QueryType get();

   private:
      Table<db::Chunk> table;

   public:
      void clear()
      {
         table.clear();
      }

      BulkHandle newBulkHandle()
      {
         return this->table.bulkInsert();
      }

      void flush(BulkHandle& handle)
      {
         handle = {};
      }

      void insert(FsckChunkList& chunks, const BulkHandle& handle)
      {
         insert(chunks, &handle);
      }
};

class FsckDBContDirsTable
{
   public:
      typedef boost::shared_ptr<Buffer<db::ContDir> > BulkHandle;

   public:
      FsckDBContDirsTable(const std::string& dbPath, size_t fragmentSize, bool allowCreate)
         : table(dbPath + "/contdirs", fragmentSize, allowCreate)
      {
      }

      void insert(FsckContDirList& contDirs, const BulkHandle* handle = NULL);

      Table<db::ContDir>::QueryType get();

   private:
      Table<db::ContDir> table;

   public:
      void clear()
      {
         table.clear();
      }

      BulkHandle newBulkHandle()
      {
         return this->table.bulkInsert();
      }

      void flush(BulkHandle& handle)
      {
         handle = {};
      }

      void insert(FsckContDirList& contDirs, const BulkHandle& handle)
      {
         insert(contDirs, &handle);
      }
};

class FsckDBFsIDsTable
{
   public:
      typedef boost::shared_ptr<Buffer<db::FsID> > BulkHandle;

   public:
      FsckDBFsIDsTable(const std::string& dbPath, size_t fragmentSize, bool allowCreate)
         : table(dbPath + "/fsids", fragmentSize, allowCreate)
      {
      }

      void insert(FsckFsIDList& fsIDs, const BulkHandle* handle = NULL);
      void remove(FsckFsIDList& fsIDs);

      Table<db::FsID>::QueryType get();

   private:
      Table<db::FsID> table;

   public:
      void clear()
      {
         this->table.clear();
      }

      BulkHandle newBulkHandle()
      {
         return this->table.bulkInsert();
      }

      void flush(BulkHandle& handle)
      {
         handle = {};
      }

      void insert(FsckFsIDList& fsIDs, const BulkHandle& handle)
      {
         insert(fsIDs, &handle);
      }
};

template<typename Data>
class FsckUniqueTable
{
   public:
      typedef boost::shared_ptr<Buffer<Data> > BulkHandle;

   protected:
      FsckUniqueTable(const std::string& name, size_t bufferSize, bool allowCreate)
         : set(name, allowCreate), bufferSize(bufferSize)
      {}

   private:
      Set<Data> set;
      size_t bufferSize;
      std::vector<boost::weak_ptr<Buffer<Data> > > bulkHandles;

   public:
      void clear()
      {
         set.clear();
         bulkHandles.clear();
      }

      BulkHandle newBulkHandle()
      {
         BulkHandle result(new Buffer<Data>(set, bufferSize) );
         bulkHandles.push_back(result);
         return result;
      }

      void flush(BulkHandle& handle)
      {
         handle = {};
      }

      SetFragmentCursor<Data> get()
      {
         if(!bulkHandles.empty() )
         {
            for(size_t i = 0; i < bulkHandles.size(); i++)
            {
               if(!bulkHandles[i].expired() )
                  throw std::runtime_error("still has open bulk handles");
            }

            bulkHandles.clear();
            set.makeUnique();
         }

         return set.cursor();
      }
};

class FsckDBUsedTargetIDsTable : public FsckUniqueTable<db::UsedTarget>
{
   public:
      FsckDBUsedTargetIDsTable(const std::string& dbPath, size_t fragmentSize, bool allowCreate)
         : FsckUniqueTable<db::UsedTarget>(dbPath + "/usedtargets", fragmentSize, allowCreate)
      {
      }

      void insert(FsckTargetIDList& targetIDs, const BulkHandle& handle);
};

class FsckDBModificationEventsTable : public FsckUniqueTable<db::ModificationEvent>
{
   public:
      FsckDBModificationEventsTable(const std::string& dbPath, size_t fragmentSize,
         bool allowCreate)
         : FsckUniqueTable<db::ModificationEvent>(dbPath + "/modevents", fragmentSize, allowCreate)
      {
      }

      void insert(const FsckModificationEventList& events, const BulkHandle& handle);
};

#endif
