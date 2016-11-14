#ifndef GROUP_H_
#define GROUP_H_

#include <boost/utility/result_of.hpp>

#include <utility>

/*
 * requires Ops : struct {
 *    typedef <key-type> KeyType;
 *    typedef <proj-type> ProjType;
 *    typedef <group-type> GroupType;
 *
 *    KeyType key(Source::ElementType&);
 *    ProjType project(Source::ElementType&);
 *    void step(Source::ElementType&);
 *
 *    GroupType finish();
 * }
 *
 * yields sequence (Source::ElementType, <row-type>)
 */
template<typename Source, typename Ops>
class Group
{
   private:
      typedef typename Ops::KeyType KeyType;
      typedef typename Ops::ProjType ProjType;
      typedef typename Ops::GroupType GroupType;

   public:
      typedef std::pair<ProjType, GroupType> ElementType;
      typedef typename Source::MarkerType MarkerType;

   public:
      Group(Source source, Ops ops)
         : source(source), ops(ops), sourceIsActive(false)
      {
      }

      bool step()
      {
         if(!this->sourceIsActive && !this->source.step() )
            return false;

         groupCurrentSet();
         return true;
      }

      ElementType* get()
      {
         return &current;
      }

      MarkerType mark() const
      {
          return this->currentMark;
      }

      void restore(MarkerType mark)
      {
         this->source.restore(mark);
         groupCurrentSet();
      }

   private:
      void groupCurrentSet()
      {
         typename Source::ElementType* row = this->source.get();

         this->currentMark = this->source.mark();
         this->current.first = this->ops.project(*row);
         this->ops.step(*row);

         KeyType currentKey = this->ops.key(*row);

         while(true)
         {
            if(!this->source.step() )
            {
               this->sourceIsActive = false;
               break;
            }

            row = this->source.get();

            if(!(currentKey == this->ops.key(*row) ) )
            {
               this->sourceIsActive = true;
               break;
            }

            this->ops.step(*row);
         }

         this->current.second = this->ops.finish();
      }

   private:
      Source source;
      Ops ops;
      ElementType current;
      typename Source::MarkerType currentMark;
      bool sourceIsActive;
};


namespace db {

template<typename Ops>
struct GroupOp
{
   Ops ops;

   template<typename Source>
   friend Group<Source, Ops> operator|(Source source, const GroupOp& fn)
   {
      return Group<Source, Ops>(source, fn.ops);
   }
};

template<typename Ops>
inline GroupOp<Ops> groupBy(Ops ops)
{
   GroupOp<Ops> result = {ops};
   return result;
}

}

#endif
