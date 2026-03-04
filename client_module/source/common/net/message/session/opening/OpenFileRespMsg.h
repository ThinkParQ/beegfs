#ifndef OPENFILERESPMSG_H_
#define OPENFILERESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/PathInfo.h>
#include <common/storage/striping/StripePattern.h>
#include <common/Common.h>

// OPENFILERESPMSG_HAS_FILESTATE is a compat feature flag that indicates the metadata server has
// serialized the fileState field as part of the response message. 
#define OPENFILERESPMSG_HAS_FILESTATE        0x01  // Bit 0

struct OpenFileRespMsg;
typedef struct OpenFileRespMsg OpenFileRespMsg;

static inline void OpenFileRespMsg_init(OpenFileRespMsg* this);

// virtual functions
extern bool OpenFileRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// inliners
static inline StripePattern* OpenFileRespMsg_createPattern(OpenFileRespMsg* this);

// getters & setters
static inline int OpenFileRespMsg_getResult(OpenFileRespMsg* this);
static inline const char* OpenFileRespMsg_getFileHandleID(OpenFileRespMsg* this);
static inline const PathInfo* OpenFileRespMsg_getPathInfo(OpenFileRespMsg* this);

struct OpenFileRespMsg
{
   NetMessage netMessage;

   int result;
   unsigned fileHandleIDLen;
   const char* fileHandleID;

   PathInfo pathInfo;

   uint32_t fileVersion;

   // for serialization
   StripePattern* pattern; // not owned by this object!

   // for deserialization
   const char* patternStart;
   uint32_t patternLength;

   // Older servers never include the fileState. Newer servers might include it if the open was
   // blocked due to FhgfsOpsErr_FILEACCESS_DENIED. Always check the compatFeatureFlag
   // OPENFILERESPMSG_HAS_FILESTATE is set to determine if fileState is actually set before using
   // this field because the default value is a valid file state (but may not be the actual state).
   uint8_t fileState;
};

extern const struct NetMessageOps OpenFileRespMsg_Ops;

void OpenFileRespMsg_init(OpenFileRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_OpenFileResp, &OpenFileRespMsg_Ops);
}

/**
 * @return must be deleted by the caller; might be of type STRIPEPATTERN_Invalid
 */
StripePattern* OpenFileRespMsg_createPattern(OpenFileRespMsg* this)
{
   return StripePattern_createFromBuf(this->patternStart, this->patternLength);
}

int OpenFileRespMsg_getResult(OpenFileRespMsg* this)
{
   return this->result;
}

const char* OpenFileRespMsg_getFileHandleID(OpenFileRespMsg* this)
{
   return this->fileHandleID;
}

const PathInfo* OpenFileRespMsg_getPathInfo(OpenFileRespMsg* this)
{
   return &this->pathInfo;
}

#endif /*OPENFILERESPMSG_H_*/
