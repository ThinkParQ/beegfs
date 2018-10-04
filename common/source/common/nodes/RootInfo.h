#ifndef ROOTINFO_H_
#define ROOTINFO_H_

#include <common/nodes/NumNodeID.h>

#include <mutex>

class RootInfo
{
   public:
      RootInfo():
         isMirrored(false)
      {
      }

      NumNodeID getID() const
      {
         std::lock_guard<std::mutex> const lock(mutex);
         return id;
      }

      bool getIsMirrored() const
      {
         std::lock_guard<std::mutex> const lock(mutex);
         return isMirrored;
      }

      void set(NumNodeID id, bool isMirrored)
      {
         std::lock_guard<std::mutex> const lock(mutex);

         this->id = id;
         this->isMirrored = isMirrored;
      }

      bool setIfDefault(NumNodeID id, bool isMirrored)
      {
         std::lock_guard<std::mutex> const lock(mutex);

         if (this->id)
            return false;

         this->id = id;
         this->isMirrored = isMirrored;
         return true;
      }

   private:
      mutable std::mutex mutex;

      NumNodeID id;
      bool isMirrored;
};

#endif
