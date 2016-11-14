#ifndef GETCLIENTSTATSMSGEX_H
#define GETCLIENTSTATSMSGEX_H

#include <common/storage/StorageErrors.h>
#include <common/net/message/nodes/GetClientStatsMsg.h>


// NOTE: The message factory requires this object to have 'deserialize' and
//       'processIncoming' methods. 'deserialize' is derived from other classes.

class GetClientStatsMsgEx : public GetClientStatsMsg
{
   public:
      GetClientStatsMsgEx() : GetClientStatsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:


   private:


};

#endif /* GETCLIENTSTATSMSGEX_H */
