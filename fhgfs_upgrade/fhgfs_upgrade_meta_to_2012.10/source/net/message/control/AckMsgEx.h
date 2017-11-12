#ifndef ACKMSGEX_H_
#define ACKMSGEX_H_

#include <common/net/message/control/AckMsg.h>

// see class AcknowledgeableMsg (fhgfs_common) for a short description

class AckMsgEx : public AckMsg
{
   public:
      AckMsgEx() : AckMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:
      
   private:

};


#endif /* ACKMSGEX_H_ */
