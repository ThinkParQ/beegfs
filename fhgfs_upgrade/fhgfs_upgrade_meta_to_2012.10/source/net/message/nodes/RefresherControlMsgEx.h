#ifndef REFRESHERCONTROLMSGEX_H_
#define REFRESHERCONTROLMSGEX_H_

#include <common/net/message/nodes/RefresherControlMsg.h>


/**
 * Starts/stops or queries status of FullRefresher component.
 */
class RefresherControlMsgEx : public RefresherControlMsg
{
   public:
      RefresherControlMsgEx() : RefresherControlMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:
};


#endif /* REFRESHERCONTROLMSGEX_H_ */
