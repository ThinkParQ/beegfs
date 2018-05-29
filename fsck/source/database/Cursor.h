#ifndef CURSOR_H_
#define CURSOR_H_

#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

template<typename Obj>
class Cursor
{
   public:
      typedef Obj ElementType;

   private:
      class SourceBase
      {
         public:
            virtual ~SourceBase() {}

            virtual bool step() = 0;
            virtual ElementType* get() = 0;
      };

      template<typename Inner>
      class Source : public SourceBase
      {
         public:
            Source(Inner inner)
               : inner(inner)
            {
            }

            bool step()
            {
               return inner.step();
            }

            ElementType* get()
            {
               return inner.get();
            }

         private:
            Inner inner;
      };

   public:
      template<typename Inner>
      explicit Cursor(Inner inner)
         : source(boost::make_shared<Source<Inner> >(inner) )
      {
      }

      bool step()
      {
         return source->step();
      }

      ElementType* get()
      {
         return source->get();
      }

   private:
      boost::shared_ptr<SourceBase> source;
};

#endif
