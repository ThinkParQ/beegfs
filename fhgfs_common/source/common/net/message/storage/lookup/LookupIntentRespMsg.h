#ifndef LOOKUPINTENTRESPMSG_H_
#define LOOKUPINTENTRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/PathInfo.h>
#include <common/storage/StatData.h>
#include <common/storage/EntryInfo.h>


/* Keep these flags in sync with the client msg flags:
   fhgfs_client_mode/source/closed/net/message/storage/LookupIntentRespMsg.h */

#define LOOKUPINTENTRESPMSG_FLAG_REVALIDATE         1 /* revalidate entry, cancel if invalid */
#define LOOKUPINTENTRESPMSG_FLAG_CREATE             2 /* create file response */
#define LOOKUPINTENTRESPMSG_FLAG_OPEN               4 /* open file response */
#define LOOKUPINTENTRESPMSG_FLAG_STAT               8 /* stat file response */


class LookupIntentRespMsg : public NetMessageSerdes<LookupIntentRespMsg>
{
   public:
      /**
       * This just prepares the basic lookup response. Use the addResponse...() methods to
       * add responses for intents.
       *
       * Serialization constructor.
       *
       * @param ownerNodeID just a reference, so do not free it as long as you use this object!
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      LookupIntentRespMsg(FhgfsOpsErr lookupResult)
         : BaseType(NETMSGTYPE_LookupIntentResp)
      {
         this->responseFlags = 0;

         this->lookupResult   = (int)lookupResult;

         this->createResult = FhgfsOpsErr_INTERNAL;

         this->entryInfoPtr = NULL;
      }

      /**
       * Deserialization constructor.
       */
      LookupIntentRespMsg() : BaseType(NETMSGTYPE_LookupIntentResp)
      {
         this->lookupResult = FhgfsOpsErr_INTERNAL;
         this->createResult = FhgfsOpsErr_INTERNAL;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->responseFlags
            % obj->lookupResult;

         if(obj->responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT)
            ctx
               % obj->statResult
               % obj->statData.serializeAs(StatDataFormat_NET);

         if(obj->responseFlags & LOOKUPINTENTRESPMSG_FLAG_REVALIDATE)
            ctx % obj->revalidateResult;

         if(obj->responseFlags & LOOKUPINTENTRESPMSG_FLAG_CREATE)
            ctx % obj->createResult;

         if(obj->responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN)
            ctx
               % obj->openResult
               % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
               % serdes::backedPtr(obj->pattern, obj->parsed.pattern)
               % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo);

         if( (obj->lookupResult == FhgfsOpsErr_SUCCESS || obj->createResult == FhgfsOpsErr_SUCCESS)
               && (ctx.isReading() || likely(obj->entryInfoPtr) ) )
            ctx % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);
      }

   private:
      int32_t responseFlags; // combination of LOOKUPINTENTRESPMSG_FLAG_...

      int32_t lookupResult;

      int32_t statResult;
      StatData statData;

      int32_t revalidateResult; // FhgfsOpsErr_SUCCESS if still valid, any other value otherwise

      int32_t createResult; // FhgfsOpsErr_...

      int32_t openResult;
      unsigned fileHandleIDLen;
      const char* fileHandleID;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object
      StripePattern* pattern;  // not owned by this object! (open file data)
      PathInfo* pathInfoPtr;   // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;
      struct {
         std::unique_ptr<StripePattern> pattern;
      } parsed;
      PathInfo pathInfo;                 // (open file data)


   public:

      // inliners

      void setLookupResult(FhgfsOpsErr lookupRes)
      {
         this->lookupResult = (int)lookupRes;
      }

      void addResponseRevalidate(FhgfsOpsErr revalidateResult)
      {
         this->responseFlags |= LOOKUPINTENTRESPMSG_FLAG_REVALIDATE;

         this->revalidateResult = (int)revalidateResult;
      }

      void addResponseCreate(FhgfsOpsErr createResult)
      {
         this->responseFlags |= LOOKUPINTENTRESPMSG_FLAG_CREATE;

         this->createResult = (int)createResult;
      }

      void addResponseOpen(FhgfsOpsErr openResult, std::string& fileHandleID,
         StripePattern* pattern, PathInfo* pathInfo)
      {
         this->responseFlags |= LOOKUPINTENTRESPMSG_FLAG_OPEN;

         this->openResult = (int)openResult;

         this->fileHandleID = fileHandleID.c_str();
         this->fileHandleIDLen = fileHandleID.length();

         this->pattern = pattern;
         this->pathInfoPtr = pathInfo;
      }

      void addResponseStat(FhgfsOpsErr statResult, StatData* statData)
      {
         this->responseFlags |= LOOKUPINTENTRESPMSG_FLAG_STAT;

         this->statResult = (int)statResult;
         this->statData   = *statData;
      }

      // getters & setters

      int getResponseFlags() const
      {
         return responseFlags;
      }

      FhgfsOpsErr getLookupResult() const { return (FhgfsOpsErr)lookupResult; }
      FhgfsOpsErr getCreateResult() const { return (FhgfsOpsErr)createResult; }
      FhgfsOpsErr getOpenResult() const { return (FhgfsOpsErr)openResult; }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      StatData* getStatData(void)
      {
         return &this->statData;
      }

      /*
       * @param entryInfo not owned by this object
       */
      void setEntryInfo(EntryInfo *entryInfo)
      {
         this->entryInfoPtr = entryInfo;
      }

};

#endif /* LOOKUPINTENTRESPMSG_H_ */
