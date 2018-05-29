#ifndef BUMPFILEVERSIONMSGEX_H
#define BUMPFILEVERSIONMSGEX_H

#include <common/net/message/session/BumpFileVersionMsg.h>
#include <common/net/message/session/BumpFileVersionRespMsg.h>
#include <net/message/MirroredMessage.h>

class BumpFileVersionMsgEx : public MirroredMessage<BumpFileVersionMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<BumpFileVersionRespMsg, NETMSGTYPE_BumpFileVersion>
         ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FileIDLock lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo().getIsBuddyMirrored(); }

   private:
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<BumpFileVersionRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "BumpFileVersionMsgEx/forward"; }
};

#endif
