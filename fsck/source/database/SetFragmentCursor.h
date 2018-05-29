#ifndef SETFRAGMENTCURSOR_H_
#define SETFRAGMENTCURSOR_H_

#include <database/SetFragment.h>

template<typename Data>
class SetFragmentCursor {
   public:
      typedef Data ElementType;
      typedef size_t MarkerType;

   private:
      SetFragment<Data>* fragment;
      size_t currentGetIndex;

   public:
      explicit SetFragmentCursor(SetFragment<Data>& fragment)
         : fragment(&fragment), currentGetIndex(-1)
      {}

      bool step()
      {
         if (currentGetIndex + 1 >= fragment->size() )
            return false;

         currentGetIndex++;
         return true;
      }

      Data* get()
      {
         return &(*fragment)[currentGetIndex];
      }

      MarkerType mark() const
      {
         return currentGetIndex;
      }

      void restore(MarkerType mark)
      {
         currentGetIndex = mark - 1;
         step();
      }
};

#endif
