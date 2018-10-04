#ifndef LOOKUPINTENTRESPMSG_H_
#define LOOKUPINTENTRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/StatData.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/PathInfo.h>
#include <common/storage/EntryInfo.h>

/**
 * This message supports only deserialization; serialization is not implemented!
 */


/* Keep these flags in sync with the userspace msg flags:
   fhgfs_common/source/common/net/message/storage/LookupIntentRespMsg.h */

#define LOOKUPINTENTRESPMSG_FLAG_REVALIDATE         1 /* revalidate entry, cancel if invalid */
#define LOOKUPINTENTRESPMSG_FLAG_CREATE             2 /* create file response */
#define LOOKUPINTENTRESPMSG_FLAG_OPEN               4 /* open file response */
#define LOOKUPINTENTRESPMSG_FLAG_STAT               8 /* stat file response */


struct LookupIntentRespMsg;
typedef struct LookupIntentRespMsg LookupIntentRespMsg;


static inline void LookupIntentRespMsg_init(LookupIntentRespMsg* this);

// virtual functions
extern bool LookupIntentRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

struct LookupIntentRespMsg
{
   NetMessage netMessage;

   int responseFlags; // combination of LOOKUPINTENTRESPMSG_FLAG_...

   int lookupResult;

   EntryInfo entryInfo; // only deserialized if either lookup or create was successful

   int statResult;
   StatData statData;

   int revalidateResult; // FhgfsOpsErr_SUCCESS if still valid, any other value otherwise

   int createResult; // FhgfsOpsErr_...

   int openResult;
   unsigned fileHandleIDLen;
   const char* fileHandleID;

   // for deserialization
   const char* patternStart; // (open file data)
   uint32_t patternLength; // (open file data)
   PathInfo pathInfo;
};

extern const struct NetMessageOps LookupIntentRespMsg_Ops;

void LookupIntentRespMsg_init(LookupIntentRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_LookupIntentResp, &LookupIntentRespMsg_Ops);
}

#endif /* LOOKUPINTENTRESPMSG_H_ */
