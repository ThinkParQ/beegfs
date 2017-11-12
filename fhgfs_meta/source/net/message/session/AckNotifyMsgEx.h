#ifndef META_ACKNOTIFYMSGEX_H
#define META_ACKNOTIFYMSGEX_H

#include <common/net/message/session/AckNotifyMsg.h>
#include <common/net/message/session/AckNotifyRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <session/MirrorMessageResponseState.h>

class AckNotifiyMsgEx : public MirroredMessage<AckNotifiyMsg, std::tuple<>>
{
   public:
      typedef ErrorCodeResponseState<
         AckNotifiyRespMsg,
         NETMSGTYPE_AckNotify> ResponseState;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext&, bool) override
      {
         // do nothing at all. MirroredMessage has taken care of everything
         return boost::make_unique<ResponseState>(FhgfsOpsErr_SUCCESS);
      }

      std::tuple<> lock(EntryLockStore&) override { return {}; }

      bool isMirrored() override { return true; }

   private:
      void forwardToSecondary(ResponseContext& ctx) override
      {
         sendToSecondary(ctx, *this, NETMSGTYPE_AckNotifyResp);
      }

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return static_cast<AckNotifiyRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "AckNotifiyMsgEx/forward"; }
};

#endif
