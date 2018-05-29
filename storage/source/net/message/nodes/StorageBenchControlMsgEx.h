#ifndef STORAGEBENCHCONTROLMSGEX_H_
#define STORAGEBENCHCONTROLMSGEX_H_

#include <common/net/message/nodes/StorageBenchControlMsg.h>
#include <common/Common.h>

class StorageBenchControlMsgEx: public StorageBenchControlMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* STORAGEBENCHCONTROLMSGEX_H_ */
