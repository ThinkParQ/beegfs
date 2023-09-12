#ifndef RENAMEV2MSGEX_H_
#define RENAMEV2MSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/moving/RenameMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <storage/DirEntry.h>
#include <storage/MetaStore.h>

struct RenameV2Locks
{
   HashDirLock toFileHashLock;

   ParentNameLock fromNameLock;
   ParentNameLock toNameLock;
   FileIDLock fromDirLock;
   FileIDLock toDirLock;
   // source file must be locked because concurrent modifications of file attributes may
   // race with the moving operation between two servers.
   FileIDLock fromFileLockF;
   FileIDLock fromFileLockD;
   // if target exists, the target file must be unlocked to exclude concurrent operations on
   // target (eg close, setxattr, ...)
   FileIDLock unlinkedFileLock;

   RenameV2Locks() = default;

   RenameV2Locks(const RenameV2Locks&) = delete;
   RenameV2Locks& operator=(const RenameV2Locks&) = delete;

   RenameV2Locks(RenameV2Locks&& other)
   {
      swap(other);
   }

   RenameV2Locks& operator=(RenameV2Locks&& other)
   {
      RenameV2Locks(std::move(other)).swap(*this);
      return *this;
   }

   void swap(RenameV2Locks& other)
   {
      std::swap(toFileHashLock, other.toFileHashLock);
      std::swap(fromNameLock, other.fromNameLock);
      std::swap(toNameLock, other.toNameLock);
      std::swap(fromDirLock, other.fromDirLock);
      std::swap(toDirLock, other.toDirLock);
      std::swap(fromFileLockF, other.fromFileLockF);
      std::swap(fromFileLockD, other.fromFileLockD);
      std::swap(unlinkedFileLock, other.unlinkedFileLock);
   }
};

class RenameV2MsgEx : public MirroredMessage<RenameMsg, RenameV2Locks>
{
   public:
      typedef ErrorCodeResponseState<RenameRespMsg, NETMSGTYPE_Rename> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      RenameV2Locks lock(EntryLockStore& store) override;

      bool isMirrored() override { return getFromDirInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FhgfsOpsErr movingPerform(DirInode& fromParent, EntryInfo* fromDirInfo,
         const std::string& oldName, DirEntryType entryType, EntryInfo* toDirInfo,
         const std::string& newName, std::string& unlinkedEntryID);

      FhgfsOpsErr renameInSameDir(DirInode& fromParent, const std::string& oldName,
         const std::string& toName, std::string& unlinkedEntryID);
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
      FhgfsOpsErr updateRenamedDirInode(EntryInfo* renamedDirEntryInfo, EntryInfo* toDirInfo);
      FhgfsOpsErr unlinkRemoteFileInode(EntryInfo* entryInfo);

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<RenameRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "RenameV2MsgEx/forward"; }
};


#endif /*RENAMEV2MSGEX_H_*/
