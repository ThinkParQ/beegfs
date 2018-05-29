#ifndef STATRESPMSG_H_
#define STATRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StatData.h>


#define STATRESPMSG_FLAG_HAS_PARENTINFO                  1 /* msg includes parentOwnerNodeID and
                                                              parentEntryID */


struct StatRespMsg;
typedef struct StatRespMsg StatRespMsg;

static inline void StatRespMsg_init(StatRespMsg* this);
static inline void StatRespMsg_getParentInfo(StatRespMsg* this, NumNodeID* outParentNodeID,
   char** outParentEntryID);

// virtual functions
extern bool StatRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);
extern unsigned StatRespMsg_getSupportedHeaderFeatureFlagsMask(NetMessage* this);

// getters & setters
static inline int StatRespMsg_getResult(StatRespMsg* this);
static inline StatData* StatRespMsg_getStatData(StatRespMsg* this);

struct StatRespMsg
{
   NetMessage netMessage;

   int result;
   StatData statData;

   const char* parentEntryID;
   NumNodeID parentNodeID;
};

extern const struct NetMessageOps StatRespMsg_Ops;

void StatRespMsg_init(StatRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_StatResp, &StatRespMsg_Ops);

   NumNodeID_set(&this->parentNodeID, 0);
   this->parentEntryID = NULL;
}

int StatRespMsg_getResult(StatRespMsg* this)
{
   return this->result;
}


StatData* StatRespMsg_getStatData(StatRespMsg* this)
{
   return &this->statData;
}

/**
 * Get parentInfo
 *
 * Note: outParentEntryID is a string copy
 */
void StatRespMsg_getParentInfo(StatRespMsg* this, NumNodeID* outParentNodeID, char** outParentEntryID)
{
   if (outParentNodeID)
   {
      *outParentNodeID = this->parentNodeID;

      if (likely(outParentEntryID) )
      *outParentEntryID = StringTk_strDup(this->parentEntryID);
   }
}

#endif /*STATRESPMSG_H_*/
