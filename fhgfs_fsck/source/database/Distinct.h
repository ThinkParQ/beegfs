#ifndef DISTINCT_H_
#define DISTINCT_H_

#include <boost/type_traits/decay.hpp>
#include <boost/utility/result_of.hpp>

template<typename Source, typename KeyExtract>
class Distinct
{
   private:
      typedef typename boost::decay<
            typename boost::result_of<KeyExtract(typename Source::ElementType&)>::type
         >::type KeyType;

   public:
      typedef typename Source::ElementType ElementType;
      typedef typename Source::MarkerType MarkerType;

   public:
      Distinct(Source source, KeyExtract keyExtract)
         : source(source), keyExtract(keyExtract), hasKey(false)
      {}

      bool step()
      {
         if(!this->source.step() )
            return false;

         while(this->hasKey && this->key == this->keyExtract(*this->source.get() ) )
         {
            if(!this->source.step() )
               return false;
         }

         this->hasKey = true;
         this->key = this->keyExtract(*this->source.get() );
         return true;
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
         this->hasKey = true;
         this->key = this->keyExtract(*this->source.get() );
      }

   private:
      Source source;
      KeyType key;
      KeyExtract keyExtract;
      bool hasKey;
};


namespace db {

template<typename KeyExtract>
struct DistinctOp
{
   KeyExtract keyExtract;

   template<typename Source>
   friend Distinct<Source, KeyExtract> operator|(Source source, const DistinctOp& op)
   {
      return Distinct<Source, KeyExtract>(source, op.keyExtract);
   }
};

template<typename KeyExtract>
inline DistinctOp<KeyExtract> distinctBy(KeyExtract keyExtract)
{
   DistinctOp<KeyExtract> result = {keyExtract};
   return result;
}

}

#endif
