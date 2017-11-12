#ifndef GETTARGETMAPPINGSMSGEX_H_
#define GETTARGETMAPPINGSMSGEX_H_

#include <common/net/message/nodes/GetTargetMappingsMsg.h>

class GetTargetMappingsMsgEx : public GetTargetMappingsMsg
{
   public:
      GetTargetMappingsMsgEx() : GetTargetMappingsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:
};


#endif /* GETTARGETMAPPINGSMSGEX_H_ */
