#ifndef LOOKUPINTENTINFO_H_
#define LOOKUPINTENTINFO_H_

#include <app/App.h>
#include <common/Common.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/nodes/Node.h>
#include <common/storage/Path.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/PathInfo.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/striping/StripePattern.h>
#include <common/toolkit/list/UInt16List.h>


struct LookupIntentInfoOut;
typedef struct LookupIntentInfoOut LookupIntentInfoOut;

static inline void LookupIntentInfoOut_initFromRespMsg(LookupIntentInfoOut* this,
   LookupIntentRespMsg* respMsg);
static inline void LookupIntentInfoOut_setEntryInfoPtr(LookupIntentInfoOut* this,
   EntryInfo* entryInfo);
static inline void  LookupIntentInfoOut_setStripePattern(LookupIntentInfoOut* this,
   StripePattern *pattern);

static inline void LookupIntentInfoOut_uninit(LookupIntentInfoOut* this);



/**
 * Out-Args for _lookupCreateStat operations. Init only with LookupIntentInfoOut_prepare().
 *
 * Note: This needs preparation by the same caller that does init of LookupIntentInfoIn
 * (to assign some out-pointers before the actual method is called).
 */
struct LookupIntentInfoOut
{
   int responseFlags; // combination of LOOKUPINTENTRESPMSG_FLAG_...

   int lookupRes;
   int statRes;
   int createRes; // FhgfsOpsErr_...
   int revalidateRes; // FhgfsOpsErr_SUCCESS if still valid, any other value otherwise
   int openRes;

   fhgfs_stat* fhgfsStat;


   EntryInfo* entryInfoPtr; // Note: Not owned by this object.
                            //       Only deserialized if either lookup or create was successful.
   int revalidateUpdatedFlags;


   unsigned fileHandleIDLen;
   const char* fileHandleID;

   // only set if open was successful
   PathInfo pathInfo;

   StripePattern* stripePattern;
};


static inline void LookupIntentInfoOut_prepare(LookupIntentInfoOut* this,
   EntryInfo* outEntryInfo, fhgfs_stat* outFhgfsStat)
{
   // note: we only assign the pointers to the out-structs here
   this->entryInfoPtr = outEntryInfo;
   this->fhgfsStat = outFhgfsStat;

   this->responseFlags = 0;

   this->lookupRes = FhgfsOpsErr_INTERNAL;
   this->statRes   = FhgfsOpsErr_INTERNAL;
   this->createRes = FhgfsOpsErr_INTERNAL;
   this->revalidateRes = FhgfsOpsErr_INTERNAL;
   this->openRes   = FhgfsOpsErr_INTERNAL;

   this->stripePattern = NULL;
}

void LookupIntentInfoOut_initFromRespMsg(LookupIntentInfoOut* this,
   LookupIntentRespMsg* respMsg)
{
   this->responseFlags = respMsg->responseFlags;

   this->lookupRes     = respMsg->lookupResult;

   if (this->responseFlags & LOOKUPINTENTRESPMSG_FLAG_CREATE)
      this->createRes     = respMsg->createResult;

   if (respMsg->responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT)
   {
      this->statRes       = respMsg->statResult;

      if (this->statRes == FhgfsOpsErr_SUCCESS)
         StatData_getOsStat(&respMsg->statData, this->fhgfsStat);
   }

   if (respMsg->responseFlags & LOOKUPINTENTRESPMSG_FLAG_REVALIDATE)
   {
      this->revalidateRes = respMsg->revalidateResult;

      /* no need to fill in EntryInfo on revalidate, we (the client) already have it. Only flags
       * might need to be updated. */
      this->revalidateUpdatedFlags = respMsg->entryInfo.featureFlags;
   }
   else
   if (respMsg->lookupResult == FhgfsOpsErr_SUCCESS || respMsg->createResult == FhgfsOpsErr_SUCCESS)
      EntryInfo_dup(&respMsg->entryInfo, this->entryInfoPtr);


   // only provided by the server on open
   if (respMsg->responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN)
   {
      this->openRes       = respMsg->openResult;

      if (respMsg->openResult == FhgfsOpsErr_SUCCESS)
      {
         this->fileHandleIDLen = respMsg->fileHandleIDLen;
         this->fileHandleID = kstrndup(respMsg->fileHandleID, respMsg->fileHandleIDLen, GFP_NOFS);

         PathInfo_dup(&respMsg->pathInfo, &this->pathInfo);

         this->stripePattern =
            StripePattern_createFromBuf(respMsg->patternStart, respMsg->patternLength);
      }
   }
}

void LookupIntentInfoOut_setEntryInfoPtr(LookupIntentInfoOut* this, EntryInfo* entryInfo)
{
   this->entryInfoPtr = entryInfo;
}

void  LookupIntentInfoOut_setStripePattern(LookupIntentInfoOut* this, StripePattern *pattern)
{
   this->stripePattern = pattern;
}

/**
 * Uninitialize (free) LookupIntentInfoOut values
 */
void LookupIntentInfoOut_uninit(LookupIntentInfoOut* this)
{
   // Free EntryInfo values
   if ( (this->entryInfoPtr) &&
        (this->createRes == FhgfsOpsErr_SUCCESS || this->lookupRes == FhgfsOpsErr_SUCCESS) )
      EntryInfo_uninit(this->entryInfoPtr);

   // Destruct PathInfo
   if (this->openRes == FhgfsOpsErr_SUCCESS)
   {  // unitialize values only set on open success and if there was an open at all
      PathInfo_uninit(&this->pathInfo);

      if (this->stripePattern)
         StripePattern_virtualDestruct(this->stripePattern);
   }
}


#endif /*LOOKUPINTENTINFO_H_*/
