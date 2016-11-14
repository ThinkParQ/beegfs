#ifndef GETHIGHRESSTATSMSGEX_H_
#define GETHIGHRESSTATSMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/GetHighResStatsMsg.h>


class GetHighResStatsMsgEx : public GetHighResStatsMsg
{
   public:
      GetHighResStatsMsgEx() : GetHighResStatsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:


   private:


};

#endif /* GETHIGHRESSTATSMSGEX_H_ */
