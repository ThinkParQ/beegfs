#ifndef RMDIRENTRYMSGEX_H_
#define RMDIRENTRYMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/RmDirEntryMsg.h>
#include <storage/MetaStore.h>

// A repair operation, called from fhgfs-ctl

class RmDirEntryMsgEx : public RmDirEntryMsg
{
   public:
      RmDirEntryMsgEx() : RmDirEntryMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

   protected:

   private:
      FhgfsOpsErr rmDirEntry(Path& path);
};

#endif /* RMDIRENTRYMSGEX_H_ */
