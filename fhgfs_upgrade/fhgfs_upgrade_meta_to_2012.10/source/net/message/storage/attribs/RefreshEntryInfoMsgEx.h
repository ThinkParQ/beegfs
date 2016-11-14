#ifndef REFRESHENTRYINFOMSGEX_H_
#define REFRESHENTRYINFOMSGEX_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoMsg.h>

// Update entry info, called by fsck or by fhgfs-ctl

class RefreshEntryInfoMsgEx : public RefreshEntryInfoMsg
{
   public:
      RefreshEntryInfoMsgEx() : RefreshEntryInfoMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

   protected:

   private:
      FhgfsOpsErr refreshInfoRec(EntryInfo* entryInfo);
      FhgfsOpsErr refreshInfoRoot();

};


#endif /* REFRESHENTRYINFOMSGEX_H_ */
