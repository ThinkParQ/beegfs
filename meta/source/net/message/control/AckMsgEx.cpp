#include <program/Program.h>
#include "AckMsgEx.h"

bool AckMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "Ack incoming";
   #endif //BEEGFS_DEBUG
   
   LOG_DEBUG(logContext, 5, std::string("Value: ") + getValue() );
   
   AcknowledgmentStore* ackStore = Program::getApp()->getAckStore();
   ackStore->receivedAck(getValue() );

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_ACK,
      getMsgHeaderUserID() );
   
   // note: this message does not require a response

   return true;
}


