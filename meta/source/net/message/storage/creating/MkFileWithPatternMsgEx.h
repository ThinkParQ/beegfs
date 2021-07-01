#ifndef MKFILEWITHPATTERNMSGEX_H_
#define MKFILEWITHPATTERNMSGEX_H_

#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/net/message/storage/creating/MkFileWithPatternMsg.h>
#include <common/net/message/storage/creating/MkFileWithPatternRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>

/**
 * Similar to class MsgHelperMkFile, but called with a create pattern, for example from fhgfs-ctl
 * or from ioctl calls.
 */
class MkFileWithPatternMsgEx : public MirroredMessage<MkFileWithPatternMsg,
   std::tuple<FileIDLock, ParentNameLock, FileIDLock>>
{
   public:
      typedef ErrorAndEntryResponseState<MkFileWithPatternRespMsg, NETMSGTYPE_MkFileWithPattern>
         ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, ParentNameLock, FileIDLock> lock(EntryLockStore& store) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      bool isMirrored() override { return getParentInfo()->getIsBuddyMirrored(); }

   private:
      MirroredTimestamps dirTimestamps;

      FhgfsOpsErr mkFile(const EntryInfo* parentInfo, MkFileDetails& mkDetails,
         EntryInfo* outEntryInfo, FileInodeStoreData& inodeDiskData);
      FhgfsOpsErr mkMetaFile(DirInode& dir, MkFileDetails& mkDetails,
         EntryInfo* outEntryInfo, FileInodeStoreData& inodeDiskData);

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<MkFileRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "MkFileWithPatternMsgEx/forward"; }

      FileInodeStoreData inodeDiskData;
      std::string entryID;
};

#endif /* MKFILEWITHPATTERNMSGEX_H_ */
