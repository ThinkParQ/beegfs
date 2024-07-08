#ifndef GETENTRYINFO_H_
#define GETENTRYINFO_H_

#include <storage/DirInode.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <net/message/MirroredMessage.h>

class GetEntryInfoMsgResponseState : public MirroredMessageResponseState
{
   public:
      GetEntryInfoMsgResponseState() : result(FhgfsOpsErr_INTERNAL), mirrorNodeID(0)
      {
      }

      GetEntryInfoMsgResponseState(GetEntryInfoMsgResponseState&& other) :
         result(other.result),
         mirrorNodeID(other.mirrorNodeID),
         pattern(std::move(other.pattern)),
         pathInfo(std::move(other.pathInfo))
      {
      }

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         GetEntryInfoRespMsg resp(result, pattern.get(), mirrorNodeID, &pathInfo);
         ctx.sendResponse(resp);
      }

      // GetEntryInfoMsgEx is transformed into a mirrored message to utilize
      // MirroredMessage::lock(), thereby preventing races with operations such
      // as unlink. However, forwarding this message to the secondary is unnecessary.
      // Overriding the changeObservableState() function to always return false ensures
      // that this message never gets forwarded to seconadary.
      bool changesObservableState() const override
      {
         return false;
      }

      void setGetEntryInfoResult(FhgfsOpsErr result) { this->result = result; }
      void setMirrorNodeID(uint16_t nodeId) { this->mirrorNodeID = nodeId; }
      void setStripePattern(StripePattern* pattern) { this->pattern.reset(pattern); }
      void setPathInfo(PathInfo const& pathInfo) { this->pathInfo = pathInfo; }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_GetEntryInfo; }
      void serializeContents(Serializer& ser) const override {}

   private:
      FhgfsOpsErr result;
      uint16_t mirrorNodeID; // metadata mirror node (0 means "none")
      std::unique_ptr<StripePattern> pattern;
      PathInfo pathInfo;
};

class GetEntryInfoMsgEx : public MirroredMessage<GetEntryInfoMsg, FileIDLock>
{
   public:
      typedef GetEntryInfoMsgResponseState ResponseState;
      virtual bool processIncoming(ResponseContext& ctx) override;
      FileIDLock lock(EntryLockStore& store) override;

      bool isMirrored() override
      {
         return getEntryInfo()->getIsBuddyMirrored();
      }

   private:
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      void forwardToSecondary(ResponseContext& ctx) override {}
      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return FhgfsOpsErr_SUCCESS;
      }

      const char* mirrorLogContext() const override { return "GetEntryInfoMsgEx/forward"; }
      FhgfsOpsErr getInfo(EntryInfo* entryInfo, StripePattern** outPattern, PathInfo* outPathInfo);
      FhgfsOpsErr getRootInfo(StripePattern** outPattern);
};

#endif /*GETENTRYINFO_H_*/
