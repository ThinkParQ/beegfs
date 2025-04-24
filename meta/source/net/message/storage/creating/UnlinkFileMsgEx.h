#pragma once

#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <session/EntryLock.h>
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
      /**
       * Execute unlink operation on primary and secondary metadata nodes.
       * Primary handles metadata removal, chunk cleanup, and event logging.
       * Secondary handles only metadata operations.
       *
       * @param ctx Response context
       * @param dir Parent directory reference. Functions guarantee directory reference
       *            release before return, including error paths.
       * @return Response state containing operation result.
       */
      std::unique_ptr<ResponseState> executePrimary(ResponseContext& ctx, DirInode& dir);
      std::unique_ptr<ResponseState> executeSecondary(ResponseContext& ctx, DirInode& dir);

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<UnlinkFileRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "UnlinkFileMsgEx/forward"; }
};
