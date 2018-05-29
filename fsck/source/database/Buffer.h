#ifndef BUFFER_H_
#define BUFFER_H_

#include <database/Set.h>

template<typename Data>
class Buffer {
   private:
      Set<Data>* set;
      SetFragment<Data>* currentFragment;
      size_t fragmentSize;

      Buffer(const Buffer&);
      Buffer& operator=(const Buffer&);

   public:
      Buffer(Set<Data>& set, size_t fragmentSize)
         : set(&set), currentFragment(NULL), fragmentSize(fragmentSize)
      {}

      ~Buffer()
      {
         flush();
      }

      void flush()
      {
         if(currentFragment)
         {
            currentFragment->flush();
            currentFragment = NULL;
         }
      }

      void append(const Data& data)
      {
         if(!currentFragment)
            currentFragment = set->newFragment();

         currentFragment->append(data);

         if(currentFragment->size() * sizeof(Data) >= fragmentSize)
            flush();
      }
};

#endif
