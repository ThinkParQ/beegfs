#ifndef STORAGERESYNCSTARTED_H_
#define STORAGERESYNCSTARTED_H_

#include <common/net/message/storage/mirroring/StorageResyncStartedMsg.h>

class StorageResyncStartedMsgEx : public StorageResyncStartedMsg
{
   public:
      StorageResyncStartedMsgEx() : StorageResyncStartedMsg()
   { }

      virtual bool processIncoming(ResponseContext& ctx);

   private:
      void pauseWorkers();
};

#endif /* STORAGERESYNCSTARTED_H_ */
