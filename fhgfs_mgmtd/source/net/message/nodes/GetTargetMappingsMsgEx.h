#ifndef GETTARGETMAPPINGSMSGEX_H_
#define GETTARGETMAPPINGSMSGEX_H_

#include <common/net/message/nodes/GetTargetMappingsMsg.h>

class GetTargetMappingsMsgEx : public GetTargetMappingsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};


#endif /* GETTARGETMAPPINGSMSGEX_H_ */
