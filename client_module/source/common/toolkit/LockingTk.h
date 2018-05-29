#ifndef LOCKINGTK_H_
#define LOCKINGTK_H_

#include <common/Common.h>
#include <common/storage/StorageDefinitions.h>


static inline const char* LockingTk_lockTypeToStr(int entryLockType);


/**
 * @param entryLockType ENTRYLOCKTYPE_...
 * @return pointer to static string (no freeing by caller required)
 */
const char* LockingTk_lockTypeToStr(int entryLockType)
{
   if(entryLockType & ENTRYLOCKTYPE_NOWAIT)
   {
      if(entryLockType & ENTRYLOCKTYPE_UNLOCK)
         return "unlock|nowait";
      else
      if(entryLockType & ENTRYLOCKTYPE_EXCLUSIVE)
         return "exclusive|nowait";
      else
      if(entryLockType & ENTRYLOCKTYPE_SHARED)
         return "shared|nowait";
      else
         return "unknown|nowait";
   }
   else
   { // waiting allowed
      if(entryLockType & ENTRYLOCKTYPE_UNLOCK)
         return "unlock|wait";
      else
      if(entryLockType & ENTRYLOCKTYPE_EXCLUSIVE)
         return "exclusive|wait";
      else
      if(entryLockType & ENTRYLOCKTYPE_SHARED)
         return "shared|wait";
      else
         return "unknown|wait";
   }
}


#endif /* LOCKINGTK_H_ */
