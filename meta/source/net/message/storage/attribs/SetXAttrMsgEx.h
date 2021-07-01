#ifndef SETXATTRMSGEX_H_
#define SETXATTRMSGEX_H_

#include <common/net/message/storage/attribs/SetXAttrMsg.h>
#include <common/net/message/storage/attribs/SetXAttrRespMsg.h>
#include <net/message/MirroredMessage.h>

class SetXAttrMsgEx : public MirroredMessage<SetXAttrMsg, std::tuple<FileIDLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<SetXAttrRespMsg, NETMSGTYPE_SetXAttr> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, FileIDLock> lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

   private:
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<SetXAttrRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "SetXAttrMsgEx/forward"; }
};

#endif /*SETXATTRMSGEX_H_*/
