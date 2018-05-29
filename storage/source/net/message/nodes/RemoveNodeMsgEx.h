#ifndef REMOVENODEMSGEX_H_
#define REMOVENODEMSGEX_H_

#include <common/net/message/nodes/RemoveNodeMsg.h>

class RemoveNodeMsgEx : public RemoveNodeMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* REMOVENODEMSGEX_H_ */
