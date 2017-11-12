#ifndef GETCHUNKFILEATTRIBSMSGEX_H_
#define GETCHUNKFILEATTRIBSMSGEX_H_

#include <common/net/message/storage/attribs/GetChunkFileAttribsMsg.h>

class GetChunkFileAttribsMsgEx : public GetChunkFileAttribsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      int getTargetFD(ResponseContext& ctx, uint16_t actualTargetID, bool* outResponseSent);
};

#endif /*GETCHUNKFILEATTRIBSMSGEX_H_*/
