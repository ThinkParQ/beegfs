#ifndef GETCHUNKFILEATTRIBSMSGEX_H_
#define GETCHUNKFILEATTRIBSMSGEX_H_

#include <common/net/message/storage/attribs/GetChunkFileAttribsMsg.h>

class StorageTarget;

class GetChunkFileAttribsMsgEx : public GetChunkFileAttribsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      int getTargetFD(const StorageTarget& target, ResponseContext& ctx, bool* outResponseSent);
};

#endif /*GETCHUNKFILEATTRIBSMSGEX_H_*/
