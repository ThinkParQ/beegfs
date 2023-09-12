#ifndef UNLINKFILEMSGEX_H_
#define UNLINKFILEMSGEX_H_

#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>

class UnlinkFileMsgEx : public MirroredMessage<UnlinkFileMsg,
   std::tuple<HashDirLock, FileIDLock, ParentNameLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<UnlinkFileRespMsg, NETMSGTYPE_UnlinkFile> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<HashDirLock, FileIDLock, ParentNameLock, FileIDLock> lock(EntryLockStore& store) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      bool isMirrored() override { return getParentInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<ResponseState> executePrimary(ResponseContext& ctx);
      std::unique_ptr<ResponseState> executeSecondary(ResponseContext& ctx);

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<UnlinkFileRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "UnlinkFileMsgEx/forward"; }
};

#endif /*UNLINKFILEMSGEX_H_*/
