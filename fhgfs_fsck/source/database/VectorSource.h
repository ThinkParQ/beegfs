#ifndef VECTORSOURCE_H_
#define VECTORSOURCE_H_

#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

template<typename Obj>
class VectorSource
{
   public:
      typedef Obj ElementType;
      typedef typename std::vector<Obj>::const_iterator MarkerType;

   public:
      explicit VectorSource(std::vector<Obj>& data)
         : content(boost::make_shared<std::vector<Obj> >() ), index(-1)
      {
         data.swap(*content);
      }

      bool step()
      {
         if(this->index == this->content->size() - 1)
            return false;

         this->index += 1;
         return true;
      }

      ElementType* get()
      {
         return &(*this->content)[this->index];
      }

      MarkerType mark() const
      {
         return this->content->begin() + this->index;
      }

      void restore(MarkerType mark)
      {
         this->index = mark - this->content->begin();
      }

   private:
      boost::shared_ptr<std::vector<Obj> > content;
      size_t index;
};

#endif
