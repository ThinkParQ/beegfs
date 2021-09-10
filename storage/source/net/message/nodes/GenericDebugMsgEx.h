#ifndef GENERICDEBUGMSGEX_H_
#define GENERICDEBUGMSGEX_H_

#include <common/net/message/nodes/GenericDebugMsg.h>


class GenericDebugMsgEx : public GenericDebugMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      std::string processCommand();

      std::string processOpListOpenFiles(std::istringstream& commandStream);
      std::string processOpVersion(std::istringstream& commandStream);
      std::string processOpMsgQueueStats(std::istringstream& commandStream);
      std::string processOpQuotaExceeded(std::istringstream& commandStream);
      std::string processOpUsedQuota(std::istringstream& commandStream);
      std::string processOpResyncQueueLen(std::istringstream& commandStream);
      std::string processOpChunkLockStoreSize(std::istringstream& commandStream);
      std::string processOpChunkLockStoreContents(std::istringstream& commandStream);
      std::string processOpSetRejectionRate(std::istringstream& commandStream);
};

#endif /* GENERICDEBUGMSGEX_H_ */
