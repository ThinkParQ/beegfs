#include <program/Program.h>
#include "AckMsgEx.h"

bool AckMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("Ack incoming");

   LOG_DEBUG_CONTEXT(log, 5, std::string("Value: ") + getValue() );
   
   AcknowledgmentStore* ackStore = Program::getApp()->getAckStore();
   ackStore->receivedAck(getValue() );
   
   // note: this message does not require a response

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), StorageOpCounter_ACK,
      getMsgHeaderUserID() );

   return true;
}


