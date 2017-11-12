#ifndef CLOSEFILEMSGEX_H_
#define CLOSEFILEMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/session/opening/CloseFileMsg.h>
#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>
#include <storage/FileInode.h>

class CloseFileMsgEx : public MirroredMessage<CloseFileMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<CloseFileRespMsg, NETMSGTYPE_CloseFile> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FileIDLock lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<ResponseState> closeFilePrimary(ResponseContext& ctx);
      std::unique_ptr<ResponseState> closeFileSecondary(ResponseContext& ctx);
      void forwardToSecondary(ResponseContext& ctx) override;
      FhgfsOpsErr closeFileAfterEarlyResponse(MetaFileHandle inode, unsigned accessFlags,
         bool* outUnlinkDisposalFile);

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<CloseFileRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "CloseFileMsgEx/forward"; }
};

#endif /*CLOSEFILEMSGEX_H_*/
