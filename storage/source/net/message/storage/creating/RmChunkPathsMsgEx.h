#ifndef RMCHUNKPATHSMSGEX_H_
#define RMCHUNKPATHSMSGEX_H_

#include <common/net/message/storage/creating/RmChunkPathsMsg.h>

class RmChunkPathsMsgEx : public RmChunkPathsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*RMCHUNKPATHSMSGEX_H_*/
