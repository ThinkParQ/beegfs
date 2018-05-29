#include <app/App.h>
#include <common/nodes/Node.h>
#include <common/toolkit/SocketTk.h>
#include <common/toolkit/ListTk.h>
#include <nodes/NodeStoreEx.h>
#include "LogMsg.h"


const struct NetMessageOps LogMsg_Ops = {
   .serializePayload = LogMsg_serializePayload,
   .deserializePayload = LogMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void LogMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   LogMsg* thisCast = (LogMsg*)this;

   // level
   Serialization_serializeInt(ctx, thisCast->level);

   // threadID
   Serialization_serializeInt(ctx, thisCast->threadID);

   // threadName
   Serialization_serializeStr(ctx, thisCast->threadNameLen, thisCast->threadName);

   // context
   Serialization_serializeStr(ctx, thisCast->contextLen, thisCast->context);

   // logMsg
   Serialization_serializeStr(ctx, thisCast->logMsgLen, thisCast->logMsg);
}

bool LogMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   LogMsg* thisCast = (LogMsg*)this;

   // level
   if(!Serialization_deserializeInt(ctx, &thisCast->level) )
      return false;

   // threadID
   if(!Serialization_deserializeInt(ctx, &thisCast->threadID) )
      return false;

   // threadName
   if(!Serialization_deserializeStr(ctx, &thisCast->threadNameLen, &thisCast->threadName) )
      return false;

   // context
   if(!Serialization_deserializeStr(ctx, &thisCast->contextLen, &thisCast->context) )
      return false;

   // logMsg
   if(!Serialization_deserializeStr(ctx, &thisCast->logMsgLen, &thisCast->logMsg) )
      return false;

   return true;
}
