#ifndef GETFILEVERSIONMSGEX_H
#define GETFILEVERSIONMSGEX_H

#include <common/net/message/session/GetFileVersionMsg.h>
#include <common/net/message/session/GetFileVersionRespMsg.h>
#include <common/net/message/session/GetFileVersionRespMsg.h>
#include <net/message/MirroredMessage.h>

class GetFileVersionMsgResponseState : public MirroredMessageResponseState
{
   public:
      GetFileVersionMsgResponseState() : result(FhgfsOpsErr_INTERNAL), version(0)
      {
      }

      GetFileVersionMsgResponseState(GetFileVersionMsgResponseState&& other) :
         result(other.result),
         version(other.version)
      {
      }

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         GetFileVersionRespMsg resp(result, version);
         ctx.sendResponse(resp);
      }

      // GetFileVersionMsgEx is transformed into a mirrored message to leverage
      // MirroredMessage::lock(), preventing races with operations such as unlink.
      // However, forwarding this message to the secondary is unnecessary.
      // Overriding the changeObservableState() function to always return false ensures
      // that this message is never forwarded unnecessarily.
      bool changesObservableState() const override
      {
         return false;
      }

      void setGetFileVersionResult(FhgfsOpsErr result) { this->result = result; }
      void setFileVersion(uint64_t version) { this->version = version; }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_GetFileVersion; }
      void serializeContents(Serializer& ser) const override {}

   private:
      FhgfsOpsErr result;
      uint64_t version;
};

class GetFileVersionMsgEx : public MirroredMessage<GetFileVersionMsg, FileIDLock>
{
   public:
      typedef GetFileVersionMsgResponseState ResponseState;
      virtual bool processIncoming(ResponseContext& ctx) override;

      FileIDLock lock(EntryLockStore& store) override;
      bool isMirrored() override
      {
         return getEntryInfo().getIsBuddyMirrored();
      }

   private:
      private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      void forwardToSecondary(ResponseContext& ctx) override {}
      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return FhgfsOpsErr_SUCCESS;
      }

      const char* mirrorLogContext() const override { return "GetFileVersionMsgEx/forward"; }
};

#endif /* GETFILEVERSIONMSGEX_H */
