#ifndef DUMMYMSGEX_H
#define DUMMYMSGEX_H

#include <common/net/message/control/DummyMsg.h>

/*
 * this is intended for testing purposes only
 * just sends another DummyMsg back to the sender
 */

class DummyMsgEx : public DummyMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      void processIncomingRoot();
};

#endif /*DUMMYMSGEX_H*/
