#pragma once

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MoveFileInodeMsg.h>
#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>
#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>

class MoveFileInodeMsgResponseState : public MirroredMessageResponseState
{
   public:
      MoveFileInodeMsgResponseState() : result(FhgfsOpsErr_INTERNAL), linkCount(0) {}

      explicit MoveFileInodeMsgResponseState(Deserializer& des)
      {
         serialize(this, des);
      }

      MoveFileInodeMsgResponseState(MoveFileInodeMsgResponseState&& other) :
         result(other.result), linkCount(other.linkCount) {}

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         MoveFileInodeRespMsg resp(result, linkCount);
         ctx.sendResponse(resp);
      }

      void setResult(FhgfsOpsErr res) { this->result = res; }
      void setHardlinkCount(unsigned linkCount) { this->linkCount = linkCount; }
      bool changesObservableState() const override { return this->result == FhgfsOpsErr_SUCCESS; }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_MoveFileInodeResp; }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
          ctx
            % obj->result
            % obj->linkCount;
      }

      void serializeContents(Serializer& ser) const override
      {
         serialize(this, ser);
      }

   private:
      FhgfsOpsErr result;
      unsigned linkCount;
};

class MoveFileInodeMsgEx : public MirroredMessage<MoveFileInodeMsg,
   std::tuple<FileIDLock, ParentNameLock, FileIDLock>>
{
   public:
      typedef MoveFileInodeMsgResponseState ResponseState;
      virtual bool processIncoming(ResponseContext& ctx) override;

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      std::tuple<FileIDLock, ParentNameLock, FileIDLock> lock(EntryLockStore& store) override;
      bool isMirrored() override { return getFromFileEntryInfo()->getIsBuddyMirrored(); }

   private:
      ResponseContext* rctx;

      void forwardToSecondary(ResponseContext& ctx) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<MoveFileInodeRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "MoveFileInodeMsgEx/forward"; }
};
