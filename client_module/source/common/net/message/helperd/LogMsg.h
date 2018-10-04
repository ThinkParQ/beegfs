#ifndef LOGMSG_H_
#define LOGMSG_H_

#include <common/net/message/NetMessage.h>


struct LogMsg;
typedef struct LogMsg LogMsg;

static inline void LogMsg_init(LogMsg* this);
static inline void LogMsg_initFromEntry(LogMsg* this, int level,
   int threadID, const char* threadName, const char* context, const char* logMsg);

// virtual functions
extern void LogMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool LogMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);


struct LogMsg
{
   NetMessage netMessage;

   int level;
   int threadID;
   unsigned threadNameLen;
   const char* threadName;
   unsigned contextLen;
   const char* context;
   unsigned logMsgLen;
   const char* logMsg;
};

extern const struct NetMessageOps LogMsg_Ops;

void LogMsg_init(LogMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_Log, &LogMsg_Ops);
}

/**
 * @param context just a reference, so do not free it as long as you use this object!
 * @param logMsg just a reference, so do not free it as long as you use this object!
 */
void LogMsg_initFromEntry(LogMsg* this, int level, int threadID, const char* threadName,
   const char* context, const char* logMsg)
{
   LogMsg_init(this);

   this->level = level;
   this->threadID = threadID;

   this->threadName = threadName;
   this->threadNameLen = strlen(threadName);

   this->context = context;
   this->contextLen = strlen(context);

   this->logMsg = logMsg;
   this->logMsgLen = strlen(logMsg);
}

#endif /*LOGMSG_H_*/
