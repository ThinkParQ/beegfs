#ifndef MAPTARGETSMSGEX_H_
#define MAPTARGETSMSGEX_H_

#include <common/net/message/nodes/MapTargetsMsg.h>

class MapTargetsMsgEx : public MapTargetsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* MAPTARGETSMSGEX_H_ */
