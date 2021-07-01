#ifndef RMDIRMSGEX_H_
#define RMDIRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/RmDirMsg.h>
#include <common/net/message/storage/creating/RmDirRespMsg.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>


class RmDirMsgEx : public MirroredMessage<RmDirMsg,
   std::tuple<HashDirLock, FileIDLock, FileIDLock, ParentNameLock>>
{
   public:
      typedef ErrorCodeResponseState<RmDirRespMsg, NETMSGTYPE_RmDir> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<HashDirLock, FileIDLock, FileIDLock, ParentNameLock>
         lock(EntryLockStore& store) override;

      static FhgfsOpsErr rmRemoteDirInode(EntryInfo* delEntryInfo);

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override
      {
         return rmDir(ctx, isSecondary);
      }

      bool isMirrored() override { return getParentInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<ResponseState> rmDir(ResponseContext& ctx, const bool isSecondary);
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<RmDirRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "RmDirMsgEx/forward"; }
};

#endif /*RMDIRMSGEX_H_*/
