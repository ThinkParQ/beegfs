#ifndef MKFILEMSGEX_H_
#define MKFILEMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MkFileMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>


class MkFileMsgEx : public MirroredMessage<MkFileMsg,
   std::tuple<FileIDLock, ParentNameLock, FileIDLock>>
{
   public:
      typedef ErrorAndEntryResponseState<MkFileRespMsg, NETMSGTYPE_MkFile> ResponseState;

      MkFileMsgEx()
         : mkDetails({}, 0, 0, 0, 0, 0)
      {
      }

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, ParentNameLock, FileIDLock> lock(EntryLockStore& store) override;
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      bool isMirrored() override { return getParentInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<ResponseState> executePrimary();
      std::unique_ptr<ResponseState> executeSecondary();
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<MkFileRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "MkFileMsgEx/forward"; }

      FileInodeStoreData inodeData;
      MkFileDetails mkDetails;
      std::string newEntryID;

      EntryInfo entryInfo;
};

#endif /*MKFILEMSGEX_H_*/
