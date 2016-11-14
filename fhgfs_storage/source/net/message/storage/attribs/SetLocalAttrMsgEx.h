#ifndef SETLOCALATTRMSGEX_H_
#define SETLOCALATTRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/SetLocalAttrMsg.h>

class SetLocalAttrMsgEx : public SetLocalAttrMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      int getTargetFD(ResponseContext& ctx, uint16_t actualTargetID, bool* outResponseSent);
      FhgfsOpsErr forwardToSecondary(ResponseContext& ctx, uint16_t actualTargetID,
         bool* outChunkLocked);
};


#endif /*SETLOCALATTRMSGEX_H_*/
