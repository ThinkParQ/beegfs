#ifndef OPENFILEMSGEX_H_
#define OPENFILEMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/session/opening/OpenFileMsg.h>
#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <storage/DirInode.h>
#include <storage/FileInode.h>
#include <net/message/MirroredMessage.h>
#include <session/EntryLock.h>

class OpenFileResponseState : public MirroredMessageResponseState
{
   public:
      OpenFileResponseState()
         : isIndirectCommErr(true)
      {
      }

      explicit OpenFileResponseState(Deserializer& des)
      {
         serialize(this, des);
      }

      OpenFileResponseState(FhgfsOpsErr result, const std::string& fileHandleID,
            const StripePattern& pattern, const PathInfo& pathInfo, uint64_t version)
         : isIndirectCommErr(false), result(result), fileHandleID(fileHandleID),
           pattern(pattern.clone()), pathInfo(pathInfo), version(version)
      {
      }

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         if (isIndirectCommErr)
            ctx.sendResponse(
                  GenericResponseMsg(
                     GenericRespMsgCode_INDIRECTCOMMERR,
                     "Communication with storage targets failed"));
         else
            ctx.sendResponse(
                  OpenFileRespMsg(
                     result, fileHandleID, pattern.get(), &pathInfo, version));
      }

      bool changesObservableState() const override
      {
         return !isIndirectCommErr && result == FhgfsOpsErr_SUCCESS;
      }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_OpenFile; }

      template<typename This, typename Ctx>
      static void serialize(This* obj, Ctx& ctx)
      {
         ctx
            % obj->isIndirectCommErr;

         if (!obj->isIndirectCommErr)
            ctx
               % serdes::as<int32_t>(obj->result)
               % obj->fileHandleID
               % obj->pattern
               % obj->pathInfo;
      }

      void serializeContents(Serializer& ser) const override
      {
         serialize(this, ser);
      }

   private:
      bool isIndirectCommErr;

      FhgfsOpsErr result;
      std::string fileHandleID;
      std::unique_ptr<StripePattern> pattern;
      PathInfo pathInfo;
      uint64_t version;
};

class OpenFileMsgEx : public MirroredMessage<OpenFileMsg, FileIDLock>
{
   public:
      typedef OpenFileResponseState ResponseState;

      bool processIncoming(ResponseContext& ctx) override;

      FileIDLock lock(EntryLockStore& store) override;
      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
            bool isSecondary) override;
      void forwardToSecondary(ResponseContext& ctx) override;

      bool isMirrored() override { return getEntryInfo()->getIsBuddyMirrored(); }

   private:
      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         return (FhgfsOpsErr) static_cast<OpenFileRespMsg&>(resp).getResult();
      }

      const char* mirrorLogContext() const override { return "OpenFileMsgEx/forward"; }

      std::string fileHandleID;
};

#endif /*OPENFILEMSGEX_H_*/
