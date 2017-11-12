#ifndef UNLINKLOCALFILEMSGEX_H_
#define UNLINKLOCALFILEMSGEX_H_

#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>

class UnlinkLocalFileMsgEx : public UnlinkLocalFileMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      int getTargetFD(ResponseContext& ctx, uint16_t actualTargetID, bool* outResponseSent);
      FhgfsOpsErr forwardToSecondary(ResponseContext& ctx, uint16_t actualTargetID,
         bool* outChunkLocked);
};

#endif /*UNLINKLOCALFILEMSGEX_H_*/
