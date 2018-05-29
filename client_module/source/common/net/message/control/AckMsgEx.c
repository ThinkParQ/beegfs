#include <common/nodes/Node.h>
#include <common/toolkit/ackstore/AcknowledgmentStore.h>
#include <common/toolkit/SocketTk.h>
#include <app/App.h>
#include "AckMsgEx.h"


bool __AckMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Ack incoming";

   AckMsgEx* thisCast = (AckMsgEx*)this;
   AcknowledgmentStore* ackStore;

   #ifdef LOG_DEBUG_MESSAGES
      const char* peer;

      peer = fromAddr ?
         SocketTk_ipaddrToStr(&fromAddr->addr) : StringTk_strDup(Socket_getPeername(sock) );
      LOG_DEBUG_FORMATTED(log, 4, logContext, "Received a AckMsg from: %s", peer);

      kfree(peer);
   #endif // LOG_DEBUG_MESSAGES

   IGNORE_UNUSED_VARIABLE(log);
   IGNORE_UNUSED_VARIABLE(logContext);

   ackStore = App_getAckStore(app);
   AcknowledgmentStore_receivedAck(ackStore, AckMsgEx_getValue(thisCast) );

   return true;
}


