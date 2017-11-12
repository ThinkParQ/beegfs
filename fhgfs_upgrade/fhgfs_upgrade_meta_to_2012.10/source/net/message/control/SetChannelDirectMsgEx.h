#ifndef SETCHANNELDIRECTMSGEX_H_
#define SETCHANNELDIRECTMSGEX_H_

#include <common/net/message/control/SetChannelDirectMsg.h>

class SetChannelDirectMsgEx : public SetChannelDirectMsg
{
   public:
      SetChannelDirectMsgEx() : SetChannelDirectMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:
      
   private:

};


#endif /*SETCHANNELDIRECTMSGEX_H_*/
