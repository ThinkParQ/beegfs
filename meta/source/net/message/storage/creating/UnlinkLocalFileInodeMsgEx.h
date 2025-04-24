#pragma once

#include <common/net/message/storage/creating/UnlinkLocalFileInodeMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileInodeRespMsg.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>

class UnlinkLocalFileInodeResponseState : public MirroredMessageResponseState
{
   public:
      UnlinkLocalFileInodeResponseState() : result(FhgfsOpsErr_INTERNAL), preUnlinkHardlinkCount(0) {}

      UnlinkLocalFileInodeResponseState(FhgfsOpsErr result, unsigned linkCount) :
         result(result), preUnlinkHardlinkCount(linkCount) {}

      explicit UnlinkLocalFileInodeResponseState(Deserializer& des)
      {
         serialize(this, des);
      }

      UnlinkLocalFileInodeResponseState(UnlinkLocalFileInodeResponseState&& other) :
         result(other.result), preUnlinkHardlinkCount(other.preUnlinkHardlinkCount) {}

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         UnlinkLocalFileInodeRespMsg resp(result, preUnlinkHardlinkCount);
         ctx.sendResponse(resp);
      }

      void setResult(FhgfsOpsErr res) { this->result = res; }
      void setPreUnlinkHardlinkCount(unsigned linkCount) { this->preUnlinkHardlinkCount = linkCount; }
      bool changesObservableState() const override { return true; }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_UnlinkLocalFileInodeResp; }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->preUnlinkHardlinkCount;
      }

      void serializeContents(Serializer& ser) const override
      {
         serialize(this, ser);
      }

   private:
      FhgfsOpsErr result;
      // The preUnlinkHardlinkCount represents the number of hardlinks that existed before
      // the actual unlink happens. This is mainly needed for event logging purposes.
      unsigned preUnlinkHardlinkCount;
};

class UnlinkLocalFileInodeMsgEx : public MirroredMessage<UnlinkLocalFileInodeMsg,
   std::tuple<HashDirLock, FileIDLock>>
{
   public:
      typedef UnlinkLocalFileInodeResponseState ResponseState;
      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      std::tuple<HashDirLock, FileIDLock> lock(EntryLockStore& store) override;

      bool isMirrored() override { return getDelEntryInfo()->getIsBuddyMirrored(); }

   private:
      ResponseContext* rctx;

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<UnlinkLocalFileInodeRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override
      {
         return "UnlinkLocalFileInodeMsgEx/forward";
      }
};
