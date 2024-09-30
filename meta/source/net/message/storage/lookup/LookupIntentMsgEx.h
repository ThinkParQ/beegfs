#ifndef LOOKUPINTENTMSGEX_H_
#define LOOKUPINTENTMSGEX_H_

#include <common/net/message/storage/lookup/LookupIntentMsg.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/nodes/OpCounterTypes.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include <storage/MetaStore.h>
#include <net/message/MirroredMessage.h>

class LookupIntentResponseState : public MirroredMessageResponseState
{
   public:
      LookupIntentResponseState() :
         responseFlags(0), lookupResult(FhgfsOpsErr_INTERNAL), statResult(FhgfsOpsErr_INTERNAL),
         revalidateResult(FhgfsOpsErr_INTERNAL), createResult(FhgfsOpsErr_INTERNAL),
         openResult(FhgfsOpsErr_INTERNAL)
      {}

      explicit LookupIntentResponseState(Deserializer& des)
      {
         serialize(this, des);
      }

      LookupIntentResponseState(LookupIntentResponseState&& other) :
         responseFlags(other.responseFlags),
         lookupResult(other.lookupResult),
         statResult(other.statResult),
         statData(other.statData),
         revalidateResult(other.revalidateResult),
         createResult(other.createResult),
         openResult(other.openResult),
         fileHandleID(std::move(other.fileHandleID)),
         entryInfo(std::move(other.entryInfo)),
         pattern(std::move(other.pattern)),
         pathInfo(std::move(other.pathInfo))
      {}

      void sendResponse(NetMessage::ResponseContext& ctx) override
      {
         LookupIntentRespMsg resp(lookupResult);

         if (responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT)
            resp.addResponseStat(statResult, &statData);

         if (responseFlags & LOOKUPINTENTRESPMSG_FLAG_REVALIDATE)
            resp.addResponseRevalidate(revalidateResult);

         if (responseFlags & LOOKUPINTENTRESPMSG_FLAG_CREATE)
            resp.addResponseCreate(createResult);

         if (responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN)
            resp.addResponseOpen(openResult, fileHandleID, pattern.get(), &pathInfo);

         resp.setEntryInfo(&entryInfo);

         ctx.sendResponse(resp);
      }

      bool changesObservableState() const override
      {
         if ((responseFlags & LOOKUPINTENTRESPMSG_FLAG_CREATE)
               && createResult == FhgfsOpsErr_SUCCESS)
            return true;
         if ((responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN) && openResult == FhgfsOpsErr_SUCCESS)
            return true;

         return false;
      }

   protected:
      uint32_t serializerTag() const override { return NETMSGTYPE_LookupIntent; }

      template<typename This, typename Ctx>
      static void serialize(This* obj, Ctx& ctx)
      {
         ctx
            % obj->responseFlags
            % serdes::as<int32_t>(obj->lookupResult);

         if (obj->responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT)
            ctx
               % obj->statData.serializeAs(StatDataFormat_NET)
               % serdes::as<int32_t>(obj->statResult);

         ctx
            % serdes::as<int32_t>(obj->revalidateResult)
            % serdes::as<int32_t>(obj->createResult);

         if (obj->responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN)
            ctx
               % serdes::as<int32_t>(obj->openResult)
               % obj->fileHandleID
               % obj->entryInfo
               % obj->pattern
               % obj->pathInfo;
      }

      void serializeContents(Serializer& ser) const override
      {
         serialize(this, ser);
      }

   private:
      int32_t responseFlags;

      FhgfsOpsErr lookupResult;

      FhgfsOpsErr statResult;
      StatData statData;

      FhgfsOpsErr revalidateResult;

      FhgfsOpsErr createResult;

      FhgfsOpsErr openResult;
      std::string fileHandleID;

      EntryInfo entryInfo;
      std::unique_ptr<StripePattern> pattern;
      PathInfo pathInfo;

   public:
      void setLookupResult(FhgfsOpsErr lookupRes) { lookupResult = lookupRes; }

      void addResponseRevalidate(FhgfsOpsErr revalidateResult)
      {
         responseFlags |= LOOKUPINTENTRESPMSG_FLAG_REVALIDATE;
         this->revalidateResult = revalidateResult;
      }

      void addResponseCreate(FhgfsOpsErr createResult)
      {
         responseFlags |= LOOKUPINTENTRESPMSG_FLAG_CREATE;
         this->createResult = createResult;
      }

      void addResponseOpen(FhgfsOpsErr openResult, std::string fileHandleID,
         std::unique_ptr<StripePattern> pattern, const PathInfo& pathInfo)
      {
         responseFlags |= LOOKUPINTENTRESPMSG_FLAG_OPEN;
         this->openResult = openResult;
         this->fileHandleID = std::move(fileHandleID);
         this->pattern = std::move(pattern);
         this->pathInfo = pathInfo;
      }

      void addResponseStat(FhgfsOpsErr statResult, const StatData& statData)
      {
         responseFlags |= LOOKUPINTENTRESPMSG_FLAG_STAT;
         this->statResult = statResult;
         this->statData = statData;
      }

      void setEntryInfo(EntryInfo value) { entryInfo = std::move(value); }
};

/**
 * This combines the normal lookup of a directory entry with intents, i.e. options to create,
 * open and stat the entry in a single message.
 *
 * Note: The intent options currently work only for files.
 */
class LookupIntentMsgEx : public MirroredMessage<LookupIntentMsg,
   std::tuple<FileIDLock, ParentNameLock, FileIDLock>>
{
   public:
      typedef LookupIntentResponseState ResponseState;

      virtual bool processIncoming(ResponseContext& ctx) override;

      std::tuple<FileIDLock, ParentNameLock, FileIDLock> lock(EntryLockStore& store) override;

      bool isMirrored() override { return getParentInfo()->getIsBuddyMirrored(); }

      const char* mirrorLogContext() const override { return "LookupIntentMsgEx/forward"; }

      std::unique_ptr<MirroredMessageResponseState> executeLocally(ResponseContext& ctx,
         bool isSecondary) override;

      FhgfsOpsErr processSecondaryResponse(NetMessage& resp) override
      {
         auto& respMsg = static_cast<LookupIntentRespMsg&>(resp);

         // since we only forward for open and create, we need only check those two.
         if ((respMsg.getResponseFlags() & LOOKUPINTENTRESPMSG_FLAG_CREATE)
               && respMsg.getCreateResult() != FhgfsOpsErr_SUCCESS)
            return respMsg.getCreateResult();

         if ((respMsg.getResponseFlags() & LOOKUPINTENTRESPMSG_FLAG_OPEN)
               && respMsg.getOpenResult() != FhgfsOpsErr_SUCCESS)
            return respMsg.getOpenResult();

         return FhgfsOpsErr_SUCCESS;
      }

   private:
      FhgfsOpsErr lookup(const std::string& parentEntryID, const std::string& entryName,
         bool isBuddyMirrored, EntryInfo* outEntryInfo, FileInodeStoreData* outInodeStoreData,
         bool& outInodeDataOutdated);
      FhgfsOpsErr revalidate(EntryInfo* diskEntryInfo);
      FhgfsOpsErr create(EntryInfo* parentInfo, const std::string& entryName,
         EntryInfo* outEntryInfo, FileInodeStoreData* outInodeData, bool isSecondary);
      FhgfsOpsErr stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData);
      FhgfsOpsErr open(EntryInfo* entryInfo, std::string* outFileHandleID,
         StripePattern** outPattern, PathInfo* outPathInfo, bool isSecondary);

      void forwardToSecondary(ResponseContext& ctx) override;

      MetaOpCounterTypes getOpCounterType();

      FileInodeStoreData inodeData;
      std::string entryID;
      FhgfsOpsErr lookupRes;
      bool inodeDataOutdated;
      EntryInfo diskEntryInfo;
};



#endif /* LOOKUPINTENTMSGEX_H_ */
