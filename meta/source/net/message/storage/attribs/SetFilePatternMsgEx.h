#pragma once

#include <net/message/MirroredMessage.h>
#include <common/net/message/storage/attribs/SetFilePatternMsg.h>
#include <common/net/message/storage/attribs/SetFilePatternRespMsg.h>

class SetFilePatternMsgEx : public MirroredMessage<SetFilePatternMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<SetFilePatternRespMsg, NETMSGTYPE_SetFilePattern> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      FileIDLock lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<SetFilePatternRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "SetFilePatternMsgEx/forward"; }
};
