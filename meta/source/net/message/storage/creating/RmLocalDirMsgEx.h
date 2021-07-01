#ifndef RMLOCALDIRMSGEX_H_
#define RMLOCALDIRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/RmLocalDirMsg.h>
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>

class RmLocalDirMsgEx : public MirroredMessage<RmLocalDirMsg, std::tuple<HashDirLock, FileIDLock>>
{
   public:
      typedef ErrorCodeResponseState<RmLocalDirRespMsg, NETMSGTYPE_RmLocalDir> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      std::tuple<HashDirLock, FileIDLock> lock(EntryLockStore& store) override;

      bool isMirrored() override { return getDelEntryInfo()->getIsBuddyMirrored(); }

   private:
      ResponseContext* rctx;

      std::unique_ptr<ResponseState> rmDir();

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<RmLocalDirRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "RmLocalDirMsgEx/forward"; }
};

#endif /*RMLOCALDIRMSGEX_H_*/
