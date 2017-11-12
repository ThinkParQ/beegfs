#ifndef RMDIRMSGEX_H_
#define RMDIRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/RmDirMsg.h>
#include <storage/MetaStore.h>


class RmDirMsgEx : public RmDirMsg
{
   public:
      RmDirMsgEx() : RmDirMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

      static FhgfsOpsErr rmLocalDir(EntryInfo* delEntryInfo);

   protected:
   
   private:
      FhgfsOpsErr rmDir();
};

#endif /*RMDIRMSGEX_H_*/
