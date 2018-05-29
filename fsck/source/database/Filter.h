#ifndef FILTER_H_
#define FILTER_H_

template<typename Source, class Pred>
class Filter
{
   public:
      typedef typename Source::ElementType ElementType;
      typedef typename Source::MarkerType MarkerType;

   public:
      Filter(Source source, Pred pred)
         : source(source), pred(pred)
      {
      }

      bool step()
      {
         while(this->source.step() )
         {
            if(this->pred(*get() ) )
               return true;
         }

         return false;
      }

      ElementType* get()
      {
         return this->source.get();
      }

      MarkerType mark() const
      {
         return this->source.mark();
      }

      void restore(MarkerType mark)
      {
         this->source.restore(mark);
      }

   private:
      Source source;
      Pred pred;
};


namespace db {

template<typename Pred>
struct FilterOp
{
   Pred pred;

   template<typename Source>
   friend Filter<Source, Pred> operator|(Source source, const FilterOp& op)
   {
      return Filter<Source, Pred>(source, op.pred);
   }
};

template<typename Pred>
inline FilterOp<Pred> where(Pred pred)
{
   FilterOp<Pred> result = {pred};
   return result;
}

}

#endif
