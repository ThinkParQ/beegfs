#ifndef LOOKUPINTENTRESPMSG_H_
#define LOOKUPINTENTRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/EntryInfo.h>


/* Keep these flags in sync with the client msg flags:
   fhgfs_client_mode/source/closed/net/message/storage/LookupIntentRespMsg.h */

#define LOOKUPINTENTRESPMSG_FLAG_REVALIDATE         1 /* revalidate entry, cancel if invalid */
#define LOOKUPINTENTRESPMSG_FLAG_CREATE             2 /* create file response */
#define LOOKUPINTENTRESPMSG_FLAG_OPEN               4 /* open file response */
#define LOOKUPINTENTRESPMSG_FLAG_STAT               8 /* stat file response */


class LookupIntentRespMsg : public NetMessage
{
   public:
      /**
       * This just prepares the basic lookup response. Use the addResponse...() methods to
       * add responses for intents.
       *
       * @param ownerNodeID just a reference, so do not free it as long as you use this object!
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      LookupIntentRespMsg(FhgfsOpsErr lookupResult)
         : NetMessage(NETMSGTYPE_LookupIntentResp)
      {
         this->responseFlags = 0;

         this->lookupResult   = (int)lookupResult;

         this->createResult = FhgfsOpsErr_INTERNAL;
}

      /**
       * Deserialization constructor.
       */
      LookupIntentRespMsg() : NetMessage(NETMSGTYPE_LookupIntentResp)
      {
         this->lookupResult = FhgfsOpsErr_INTERNAL;
         this->createResult = FhgfsOpsErr_INTERNAL;
      }


   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         unsigned msgLength = NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // responseFlags
            Serialization::serialLenInt();  // lookupResult

         if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT)
         {
            msgLength +=
               Serialization::serialLenInt() + // statResult
               this->statData.serialLen();    // statData
         }

         if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_REVALIDATE)
         {
            msgLength += Serialization::serialLenInt(); // revalidateResult
         }

         if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_CREATE)
         {
            msgLength += Serialization::serialLenInt(); // createResult
         }

         if(this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN)
         {
            msgLength += Serialization::serialLenInt() + // openResult
               Serialization::serialLenStrAlign4(fileHandleIDLen) +
               pattern->getSerialPatternLength();
         }

         // we only need to serialize it, if either lookup or create was successful
         if ( (this->lookupResult == FhgfsOpsErr_SUCCESS) ||
              (this->createResult == FhgfsOpsErr_SUCCESS) )
            msgLength +=  this->entryInfoPtr->serialLen();
         else
         {
            // std::cerr << "lookupResult: " << this->lookupResult << " createResult " <<
            // this->createResult << std::endl;
         }

         return msgLength;
      }

   private:
      int responseFlags; // combination of LOOKUPINTENTRESPMSG_FLAG_...

      int lookupResult;

      int statResult;
      StatData statData;

      int revalidateResult; // FhgfsOpsErr_SUCCESS if still valid, any other value otherwise

      int createResult; // FhgfsOpsErr_...

      int openResult;
      unsigned fileHandleIDLen;
      const char* fileHandleID;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object
      StripePattern* pattern; // not owned by this object! (open file data)

      // for deserialization
      EntryInfo entryInfo;
      StripePatternHeader patternHeader; // (open file data)
      const char* patternStart; // (open file data)


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
         StripePattern* pattern)
      {
         this->responseFlags |= LOOKUPINTENTRESPMSG_FLAG_OPEN;

         this->openResult = (int)openResult;

         this->fileHandleID = fileHandleID.c_str();
         this->fileHandleIDLen = fileHandleID.length();

         this->pattern = pattern;
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

      FhgfsOpsErr getLookupResult() const
      {
         return (FhgfsOpsErr)lookupResult;
      }

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
