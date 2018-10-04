#ifndef MESSAGINGTKARGS_H_
#define MESSAGINGTKARGS_H_

#include <common/Common.h>
#include <common/Types.h>
#include <toolkit/NoAllocBufferStore.h>


#define REQUESTRESPONSEARGS_FLAG_ALLOWSTATESLEEP      2 // sleep on non-{good,offline} state

#define REQUESTRESPONSEARGS_LOGFLAG_PEERTRYAGAIN                 1 // peer asked for retry
#define REQUESTRESPONSEARGS_LOGFLAG_PEERINDIRECTCOMMM            2 /* peer encountered indirect comm
                                                                      error, suggests retry */
#define REQUESTRESPONSEARGS_LOGFLAG_PEERINDIRECTCOMMM_NOTAGAIN   4 /* peer encountered indirect comm
                                                                      error, suggests no retry */
#define REQUESTRESPONSEARGS_LOGFLAG_CONNESTABLISHFAILED          8 // unable to establish connection
#define REQUESTRESPONSEARGS_LOGFLAG_COMMERR                     16 // communication error
#define REQUESTRESPONSEARGS_LOGFLAG_RETRY                       32 // communication retry


struct TargetMapper; // forward declaration
struct TargetStateStore; // forward declaration
struct NodeStoreEx; // forward declaration
struct MirrorBuddyGroupMapper; // forward declaration


struct RequestResponseNode;
typedef struct RequestResponseNode RequestResponseNode;

enum MessagingTkBufType;
typedef enum MessagingTkBufType MessagingTkBufType;

struct RequestResponseArgs;
typedef struct RequestResponseArgs RequestResponseArgs;


static inline void RequestResponseArgs_prepare(RequestResponseArgs* this, Node* node,
   NetMessage* requestMsg, unsigned respMsgType);
static inline void RequestResponseArgs_freeRespBuffers(RequestResponseArgs* this, App* app);


struct RRPeer
{
   union {
      NumNodeID target;
      uint32_t group;
   } address;
   bool isMirrorGroup;
};

static inline struct RRPeer RRPeer_target(NumNodeID target)
{
   struct RRPeer result = {
      .address = {
         .target = target,
      },
      .isMirrorGroup = false,
   };
   return result;
}

static inline struct RRPeer RRPeer_group(uint32_t group)
{
   struct RRPeer result = {
      .address = {
         .group = group,
      },
      .isMirrorGroup = true,
   };
   return result;
}

/**
 * This value container contains arguments for requestResponse() with a certain node.
 */
struct RequestResponseNode
{
   struct RRPeer peer;
   struct NodeStoreEx* nodeStore; // for nodeID lookup

   struct TargetStateStore* targetStates; /* if !NULL, check for state "good" or fail immediately if
                                          offline (other states handling depend on mirrorBuddies) */
   struct MirrorBuddyGroupMapper* mirrorBuddies; // if !NULL, the given targetID is a mirror groupID
};

enum MessagingTkBufType
{
   MessagingTkBufType_BufStore = 0,
   MessagingTkBufType_kmalloc  = 1, // only for small response messages (<4KiB)
};

/**
 * Arguments for request-response communication.
 */
struct RequestResponseArgs
{
   Node* node; // receiver

   NetMessage* requestMsg;
   unsigned respMsgType; // expected type of response message

   unsigned numRetries; // 0 means infinite retries
   unsigned rrFlags; // combination of REQUESTRESPONSEARGS_FLAG_... flags
   MessagingTkBufType respBufType; // defines how to get/alloc outRespBuf

   char* outRespBuf;
   NetMessage* outRespMsg;

   // internal (initialized by MessagingTk_requestResponseWithRRArgs() )
   unsigned char logFlags; // REQUESTRESPONSEARGS_LOGFLAG_... combination to avoid double-logging
};

/**
 * Default initializer.
 * Some of the default values will overridden by the corresponding MessagingTk methods.
 *
 * @param node may be NULL, depending on which requestResponse...() method is used.
 * @param respMsgType expected type of response message (NETMSGTYPE_...)
 */
void RequestResponseArgs_prepare(RequestResponseArgs* this, Node* node, NetMessage* requestMsg,
   unsigned respMsgType)
{
   this->node = node;

   this->requestMsg = requestMsg;
   this->respMsgType = respMsgType;

   this->numRetries = 1;
   this->rrFlags = 0;
   this->respBufType = MessagingTkBufType_BufStore;

   this->outRespBuf = NULL;
   this->outRespMsg = NULL;

   this->logFlags = 0;
}

/**
 * Free/release outRespBuf and desctruct outRespMsg if they are not NULL.
 */
void RequestResponseArgs_freeRespBuffers(RequestResponseArgs* this, App* app)
{
   NETMESSAGE_FREE(this->outRespMsg);

   if(this->outRespBuf)
   {
      if(this->respBufType == MessagingTkBufType_BufStore)
      {
         NoAllocBufferStore* bufStore = App_getMsgBufStore(app);
         NoAllocBufferStore_addBuf(bufStore, this->outRespBuf);
      }
      else
         SAFE_KFREE(this->outRespBuf);
   }
}

#endif /* MESSAGINGTKARGS_H_ */
