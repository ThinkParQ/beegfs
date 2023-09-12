#ifndef DATAGRAMLISTENER_H_
#define DATAGRAMLISTENER_H_

#include <common/components/AbstractDatagramListener.h>

class DatagramListener : public AbstractDatagramListener
{
   public:
      DatagramListener(NetFilter* netFilter, NicAddressList& localNicList,
         AcknowledgmentStore* ackStore, unsigned short udpPort,
         bool restrictOutboundInterfaces);

   protected:
      virtual void handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg);

   private:

};

#endif /*DATAGRAMLISTENER_H_*/
