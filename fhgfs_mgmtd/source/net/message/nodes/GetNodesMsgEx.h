#ifndef GETNODESMSGEX_H_
#define GETNODESMSGEX_H_

#include <common/net/message/nodes/GetNodesMsg.h>

class GetNodesMsgEx : public GetNodesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*GETNODESMSGEX_H_*/
