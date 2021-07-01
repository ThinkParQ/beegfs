#ifndef REMOVEXATTRMSGEX_H_
#define REMOVEXATTRMSGEX_H_

#include <common/net/message/storage/attribs/RemoveXAttrMsg.h>
#include <common/net/message/storage/attribs/RemoveXAttrRespMsg.h>
#include <net/message/MirroredMessage.h>

class RemoveXAttrMsgEx : public MirroredMessage<RemoveXAttrMsg, std::tuple<FileIDLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<RemoveXAttrRespMsg, NETMSGTYPE_RemoveXAttr> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, FileIDLock> lock(EntryLockStore& store) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<RemoveXAttrRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "RemoveXAttrMsgEx/forward"; }
};

#endif /*REMOVEXATTRMSGEX_H_*/

