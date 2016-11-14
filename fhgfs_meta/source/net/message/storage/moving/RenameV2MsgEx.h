#ifndef RENAMEV2MSGEX_H_
#define RENAMEV2MSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/moving/RenameMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <storage/DirEntry.h>
#include <storage/MetaStore.h>

class RenameV2MsgEx : public MirroredMessage<RenameMsg,
   std::tuple<DirIDLock, DirIDLock, ParentNameLock, ParentNameLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<RenameRespMsg, NETMSGTYPE_Rename> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<DirIDLock, DirIDLock, ParentNameLock, ParentNameLock, FileIDLock> lock(
         EntryLockStore& store) override;

      bool isMirrored() override { return getFromDirInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FhgfsOpsErr movingPerform(DirInode& fromParent, EntryInfo* fromDirInfo,
         const std::string& oldName, DirEntryType entryType, EntryInfo* toDirInfo,
         const std::string& newName, std::string& unlinkedEntryID);

      FhgfsOpsErr renameInSameDir(DirInode& fromParent, const std::string& oldName,
         const std::string& newName, std::string& unlinkedEntryID);
      FhgfsOpsErr renameDir(DirInode& fromParent, EntryInfo* fromDirInfo,
         const std::string& oldName, EntryInfo* toDirInfo, const std::string& newName);
      FhgfsOpsErr renameFile(DirInode& fromParent, EntryInfo* fromDirInfo,
         const std::string& oldName, EntryInfo* toDirInfo, const std::string& newName,
         std::string& unlinkedEntryID);

      FhgfsOpsErr remoteFileInsertAndUnlink(EntryInfo* fromFileInfo, EntryInfo* toDirInfo,
         const std::string newName, char* serialBuf, size_t serialBufLen,
         StringVector xattrs, std::string& unlinkedEntryID);
      FhgfsOpsErr remoteDirInsert(EntryInfo* toDirInfo, const std::string& newName,
         char* serialBuf, size_t serialBufLen);
      FhgfsOpsErr updateRenamedDirInode(EntryInfo* renamedEntryInfo, EntryInfo* toDirInfo);

      bool forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<RenameRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "RenameV2MsgEx/forward"; }
};


#endif /*RENAMEV2MSGEX_H_*/
