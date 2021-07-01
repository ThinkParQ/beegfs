#ifndef MKDIRMSGEX_H_
#define MKDIRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MkDirMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>


class MkDirMsgEx : public MirroredMessage<MkDirMsg, std::tuple<HashDirLock, FileIDLock, ParentNameLock>>
{
   public:
      typedef ErrorAndEntryResponseState<MkDirRespMsg, NETMSGTYPE_MkDir> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<HashDirLock, FileIDLock, ParentNameLock> lock(EntryLockStore& store) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      bool isMirrored() override { return getParentInfo()->getIsBuddyMirrored(); }

   private:
      std::string entryID;

      std::unique_ptr<ResponseState> mkDirPrimary(ResponseContext& ctx);
      std::unique_ptr<ResponseState> mkDirSecondary();

      FhgfsOpsErr mkDirDentry(DirInode& parentDir, const std::string& name,
         const EntryInfo* entryInfo, const bool isBuddyMirrored);

      FhgfsOpsErr mkRemoteDirInode(DirInode& parentDir, const std::string& name,
         EntryInfo* entryInfo, const CharVector& defaultACLXAttr, const CharVector& accessACLXAttr);
      FhgfsOpsErr mkRemoteDirCompensate(EntryInfo* entryInfo);

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<MkDirRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "MkDirMsgEx/forward"; }

      EntryInfo newEntryInfo;
};

#endif /*MKDIRMSGEX_H_*/
