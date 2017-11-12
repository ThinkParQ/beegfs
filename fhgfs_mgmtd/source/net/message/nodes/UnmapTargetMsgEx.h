#ifndef UNMAPTARGETMSGEX_H_
#define UNMAPTARGETMSGEX_H_

#include <common/net/message/nodes/UnmapTargetMsg.h>

class UnmapTargetMsgEx : public UnmapTargetMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* UNMAPTARGETMSGEX_H_ */
