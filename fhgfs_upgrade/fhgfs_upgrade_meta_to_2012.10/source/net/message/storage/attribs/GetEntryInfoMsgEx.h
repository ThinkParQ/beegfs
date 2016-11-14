#ifndef GETENTRYINFO_H_
#define GETENTRYINFO_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>

class GetEntryInfoMsgEx : public GetEntryInfoMsg
{
   public:
      GetEntryInfoMsgEx() : GetEntryInfoMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr getInfoRec(EntryInfo* entryInfo, StripePattern** outPattern);
      FhgfsOpsErr getInfoRoot(StripePattern** outPattern);

};


#endif /*GETENTRYINFO_H_*/
