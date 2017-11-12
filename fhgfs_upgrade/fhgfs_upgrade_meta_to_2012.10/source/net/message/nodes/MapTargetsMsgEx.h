#ifndef MAPTARGETSMSGEX_H_
#define MAPTARGETSMSGEX_H_

#include <common/net/message/nodes/MapTargetsMsg.h>

class MapTargetsMsgEx : public MapTargetsMsg
{
   public:
      MapTargetsMsgEx() : MapTargetsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);


   protected:
};


#endif /* MAPTARGETSMSGEX_H_ */
