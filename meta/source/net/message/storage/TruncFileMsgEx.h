#ifndef TRUNCFILEMSGEX_H_
#define TRUNCFILEMSGEX_H_

#include <common/net/message/storage/TruncFileMsg.h>
#include <common/net/message/storage/TruncFileRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <storage/MetaStore.h>


class TruncFileMsgEx : public MirroredMessage<TruncFileMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<TruncFileRespMsg, NETMSGTYPE_TruncFile> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FileIDLock lock(EntryLockStore& store) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<TruncFileRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "TruncFileMsgEx/forward"; }
};


#endif /*TRUNCFILEMSGEX_H_*/
