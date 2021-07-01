#ifndef SETDIRPATTERNMSGEX_H_
#define SETDIRPATTERNMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/SetDirPatternMsg.h>
#include <common/net/message/storage/attribs/SetDirPatternRespMsg.h>
#include <net/message/MirroredMessage.h>

// set stripe pattern, called by fhgfs-ctl

class SetDirPatternMsgEx : public MirroredMessage<SetDirPatternMsg, FileIDLock>
{
   public:
      typedef ErrorCodeResponseState<SetDirPatternRespMsg, NETMSGTYPE_SetDirPattern> ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      FileIDLock lock(EntryLockStore& store) override
      {
         return {&store, getEntryInfo()->getEntryID(), true};
      }

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;
      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<SetDirPatternRespMsg&>(resp).getValue();
      }

      const char* mirrorLogContext() const override { return "SetDirPatternMsgEx/forward"; }
};


#endif /*SETDIRPATTERNMSGEX_H_*/
