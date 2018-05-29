#ifndef SELECT_H_
#define SELECT_H_

#include <boost/utility/result_of.hpp>

template<typename Source, typename Fn>
class Select
{
   public:
      typedef typename boost::result_of<Fn(typename Source::ElementType&)>::type ElementType;
      typedef typename Source::MarkerType MarkerType;

   public:
      Select(Source source, Fn fn)
         : source(source), fn(fn)
      {
      }

      bool step()
      {
         if(!this->source.step() )
            return false;

         this->current = this->fn(*this->source.get() );
         return true;
      }

      ElementType* get()
      {
         return &current;
      }

      MarkerType mark() const
      {
         return this->source.mark();
      }

      void restore(MarkerType mark)
      {
         this->source.restore(mark);
         this->current = this->fn(*this->source.get() );
      }

   private:
      Source source;
      Fn fn;
      ElementType current;
};


namespace db {

template<typename Pred>
struct SelectOp
{
   Pred pred;

   template<typename Source>
   friend Select<Source, Pred> operator|(Source source, const SelectOp& op)
   {
      return Select<Source, Pred>(source, op.pred);
   }
};

template<typename Pred>
inline SelectOp<Pred> select(Pred pred)
{
   SelectOp<Pred> result = {pred};
   return result;
}

}

#endif
