#ifndef MSGHELPERLOCKENTRY_H_
#define MSGHELPERLOCKENTRY_H_

#include <common/storage/EntryInfo.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include <session/SessionFileStore.h>


/**
 * Common helpers for locking related messages.
 */
class MsgHelperLocking
{
   public:
      static FhgfsOpsErr trySesssionRecovery(EntryInfo* entryInfo, NumNodeID clientID,
         unsigned ownerFD, SessionFileStore* sessionFiles, SessionFile** outSessionFile);

      static FhgfsOpsErr flockAppend(EntryInfo* entryInfo, unsigned ownerFD,
         EntryLockDetails& lockDetails);


   private:
      MsgHelperLocking() {}

};


#endif /* MSGHELPERLOCKENTRY_H_ */
