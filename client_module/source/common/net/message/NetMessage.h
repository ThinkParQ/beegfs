#ifndef NETMESSAGE_H_
#define NETMESSAGE_H_

#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/Socket.h>
#include <common/toolkit/Serialization.h>
#include <common/Common.h>
#include "NetMessageTypes.h"

/**
 * Note: This "class" is not meant to be instantiated directly (consider it to be abstract).
 * It contains some "virtual" methods, as you can see in the struct NetMessage. Only the virtual
 * Method processIncoming(..) has a default implementation.
 * Derived classes have a destructor with a NetMessage-Pointer (instead of the real type)
 * because of the generic virtual destructor signature.
 * Derived classes normally have two constructors: One has no arguments and is used for
 * deserialization. The other one is the standard constructor.
 */

// common message constants
// ========================
#define NETMSG_PREFIX            ((0x42474653ULL << 32) + BEEGFS_DATA_VERSION)
#define NETMSG_MIN_LENGTH        NETMSG_HEADER_LENGTH
#define NETMSG_HEADER_LENGTH     40 /* length of the header (see struct NetMessageHeader) */
#define NETMSG_MAX_MSG_SIZE      65536    // 64kB
#define NETMSG_MAX_PAYLOAD_SIZE  ((unsigned)(NETMSG_MAX_MSG_SIZE - NETMSG_HEADER_LENGTH))

#define NETMSG_DEFAULT_USERID    (~0) // non-zero to avoid mixing up with root userID


// forward declaration
struct App;


struct NetMessageHeader;
typedef struct NetMessageHeader NetMessageHeader;
struct NetMessage;
typedef struct NetMessage NetMessage;

struct NetMessageOps;

static inline void NETMESSAGE_FREE(NetMessage* this);

static inline void NetMessage_init(NetMessage* this, unsigned short msgType,
   const struct NetMessageOps* ops);

extern void __NetMessage_deserializeHeader(DeserializeCtx* ctx, struct NetMessageHeader* outHeader);
extern void __NetMessage_serializeHeader(NetMessage* this, SerializeCtx* ctx, bool zeroLengthField);

extern void _NetMessage_serializeDummy(NetMessage* this, SerializeCtx* ctx);
extern bool _NetMessage_deserializeDummy(NetMessage* this, DeserializeCtx* ctx);


static inline unsigned NetMessage_extractMsgLengthFromBuf(const char* recvBuf);
static inline bool NetMessage_serialize(NetMessage* this, char* buf, size_t bufLen);
static inline bool NetMessage_checkHeaderFeatureFlagsCompat(NetMessage* this);

// virtual functions
extern bool NetMessage_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);
extern unsigned NetMessage_getSupportedHeaderFeatureFlagsMask(NetMessage* this);

// getters & setters
static inline unsigned short NetMessage_getMsgType(NetMessage* this);
static inline unsigned NetMessage_getMsgHeaderFeatureFlags(NetMessage* this);
static inline bool NetMessage_isMsgHeaderFeatureFlagSet(NetMessage* this, unsigned flag);
static inline void NetMessage_addMsgHeaderFeatureFlag(NetMessage* this, unsigned flag);
static inline unsigned NetMessage_getMsgLength(NetMessage* this);
static inline void NetMessage_setMsgHeaderUserID(NetMessage* this, unsigned userID);
static inline void NetMessage_setMsgHeaderTargetID(NetMessage* this, uint16_t userID);

static inline void _NetMessage_setMsgType(NetMessage* this, unsigned short msgType);


#define MSGHDRFLAG_BUDDYMIRROR_SECOND  (0x01)
#define MSGHDRFLAG_IS_SELECTIVE_ACK    (0x02)
#define MSGHDRFLAG_HAS_SEQUENCE_NO     (0x04)

struct NetMessageHeader
{
   unsigned       msgLength; // in bytes
   uint16_t       msgFeatureFlags; // feature flags for derived messages (depend on msgType)
   uint8_t        msgCompatFeatureFlags; /* for derived messages, similar to msgFeatureFlags, but
                                            "compat" because there is no check whether receiver
                                            understands these flags, so they might be ignored. */
   uint8_t        msgFlags;
// char*          msgPrefix; // NETMSG_PREFIX_STR (8 bytes)
   unsigned short msgType; // the type of payload, defined as NETMSGTYPE_x
   uint16_t       msgTargetID; // targetID (not groupID) for per-target workers on storage server
   unsigned       msgUserID; // system user ID for per-user msg queues, stats etc.
   uint64_t       msgSequence; // for retries, 0 if not present
   uint64_t       msgSequenceDone; // a sequence number that has been fully processed, or 0
};

struct NetMessageOps
{
   void (*serializePayload) (NetMessage* this, SerializeCtx* ctx);
   bool (*deserializePayload) (NetMessage* this, DeserializeCtx* ctx);

   bool (*processIncoming) (NetMessage* this, struct App* app, fhgfs_sockaddr_in* fromAddr,
      struct Socket* sock, char* respBuf, size_t bufLen);
   unsigned (*getSupportedHeaderFeatureFlagsMask) (NetMessage* this);

   void (*release)(NetMessage* this);


   // not strictly operations, but these are common to all messages and do not warrant their own
   // functions
   bool supportsSequenceNumbers;
};

struct NetMessage
{
   struct NetMessageHeader msgHeader;

   const struct NetMessageOps* ops;
};


void NetMessage_init(NetMessage* this, unsigned short msgType, const struct NetMessageOps* ops)
{
   memset(this, 0, sizeof(*this) ); // clear function pointers etc.

   // this->msgLength = 0; // zero'ed by memset
   // this->msgFeatureFlags = 0; // zero'ed by memset

   this->msgHeader.msgType = msgType;

   // needs to be set to actual ID by some async flushers etc
   this->msgHeader.msgUserID = FhgfsCommon_getCurrentUserID();

   // this->msgTargetID = 0; // zero'ed by memset

   this->ops = ops;
}

#define NETMESSAGE_CONSTRUCT(TYPE) \
   ({ \
      TYPE* msg = os_kmalloc(sizeof(TYPE)); \
      TYPE##_init(msg); \
      (NetMessage*)msg; \
   })

static inline void NETMESSAGE_FREE(NetMessage* msg)
{
   if (msg->ops->release)
      msg->ops->release(msg);

   kfree(msg);
}


/**
 * recvBuf must be at least NETMSG_MIN_LENGTH long
 */
unsigned NetMessage_extractMsgLengthFromBuf(const char* recvBuf)
{
   unsigned msgLength;
   DeserializeCtx ctx = { recvBuf, sizeof(msgLength) };

   Serialization_deserializeUInt(&ctx, &msgLength);

   return msgLength;
}

bool NetMessage_serialize(NetMessage* this, char* buf, size_t bufLen)
{
   SerializeCtx ctx = {
      .data = buf,
   };

   if(unlikely(bufLen < NetMessage_getMsgLength(this) ) )
      return false;

   __NetMessage_serializeHeader(this, &ctx, false);
   this->ops->serializePayload(this, &ctx);

   return true;
}

/**
 * Check if the msg sender has set an incompatible feature flag.
 *
 * @return false if an incompatible feature flag was set
 */
bool NetMessage_checkHeaderFeatureFlagsCompat(NetMessage* this)
{
   unsigned unsupportedFlags = ~(this->ops->getSupportedHeaderFeatureFlagsMask(this) );
   if(unlikely(this->msgHeader.msgFeatureFlags & unsupportedFlags) )
      return false; // an unsupported flag was set

   return true;
}

unsigned short NetMessage_getMsgType(NetMessage* this)
{
   return this->msgHeader.msgType;
}

unsigned NetMessage_getMsgHeaderFeatureFlags(NetMessage* this)
{
   return this->msgHeader.msgFeatureFlags;
}

/**
 * Test flag. (For convenience and readability.)
 *
 * @return true if given flag is set.
 */
bool NetMessage_isMsgHeaderFeatureFlagSet(NetMessage* this, unsigned flag)
{
   return (this->msgHeader.msgFeatureFlags & flag) != 0;
}

/**
 * Add another flag without clearing the previously set flags.
 *
 * Note: The receiver will reject this message if it doesn't know the given feature flag.
 */
void NetMessage_addMsgHeaderFeatureFlag(NetMessage* this, unsigned flag)
{
   this->msgHeader.msgFeatureFlags |= flag;
}

unsigned NetMessage_getMsgLength(NetMessage* this)
{
   if(!this->msgHeader.msgLength)
   {
      SerializeCtx ctx = { NULL, 0 };

      __NetMessage_serializeHeader(this, &ctx, true);
      this->ops->serializePayload(this, &ctx);

      this->msgHeader.msgLength = ctx.length;
   }

   return this->msgHeader.msgLength;
}

void NetMessage_setMsgHeaderUserID(NetMessage* this, unsigned userID)
{
   this->msgHeader.msgUserID = userID;
}

/**
 * @param targetID this has to be an actual targetID (not a groupID).
 */
void NetMessage_setMsgHeaderTargetID(NetMessage* this, uint16_t targetID)
{
   this->msgHeader.msgTargetID = targetID;
}

void _NetMessage_setMsgType(NetMessage* this, unsigned short msgType)
{
   this->msgHeader.msgType = msgType;
}


#endif /*NETMESSAGE_H_*/
