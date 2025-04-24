#pragma once

#include <net/message/MirroredMessage.h>
#include <common/net/message/storage/attribs/SetFileDataStateMsg.h>
#include <common/net/message/storage/attribs/SetFileDataStateRespMsg.h>

class SetFileDataStateMsgEx : public MirroredMessage<SetFileDataStateMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<SetFileDataStateRespMsg, NETMSGTYPE_SetFileDataState> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      FileIDLock lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<SetFileDataStateRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "SetFileDataStateMsgEx/forward"; }
};