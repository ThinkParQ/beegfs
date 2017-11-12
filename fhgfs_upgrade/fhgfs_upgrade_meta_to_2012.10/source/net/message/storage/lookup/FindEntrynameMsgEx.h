#ifndef FindEntrynameMSGEX_H
#define FindEntrynameMSGEX_H

#include <common/net/message/storage/lookup/FindEntrynameMsg.h>
#include <common/net/message/storage/lookup/FindEntrynameRespMsg.h>

/* This is incremental because we need to make sure that we do not run into any timeouts
 *
 */

// the maximum time for one message (in seconds)
#define MAX_SECONDS_PER_MSG 3

class FindEntrynameMsgEx : public FindEntrynameMsg
{
   public:
      FindEntrynameMsgEx() : FindEntrynameMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
         size_t bufLen, HighResolutionStats* stats) throw (SocketException);

   protected:
      
   private:
};

#endif /*FindEntrynameMSGEX_H*/
