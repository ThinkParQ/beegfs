#ifndef REGISTRATIONDATAGRAMLISTENER_H_
#define REGISTRATIONDATAGRAMLISTENER_H_

#include <common/components/AbstractDatagramListener.h>
#include <common/Common.h>

/**
 * This is a minimalistic version of a datagram listener, which only handles incoming heartbeat
 * messages.
 * This component is required during registration to wait for a management server heartbeat, but to
 * also make sure that we don't receive/handle an yincoming messages that could access data
 * structures (e.g. app->localNode), which are not fully initialized prior to registration
 * completion.
 */
class RegistrationDatagramListener : public AbstractDatagramListener
{
   public:
      RegistrationDatagramListener(NetFilter* netFilter, NicAddressList& localNicList,
         AcknowledgmentStore* ackStore, unsigned short udpPort,
         bool restrictOutboundInterfaces);

   protected:
      virtual void handleIncomingMsg(struct sockaddr_in* fromAddr, NetMessage* msg);

   private:

};

#endif /* REGISTRATIONDATAGRAMLISTENER_H_ */
