#ifndef FSYNCLOCALFILERESPMSG_H_
#define FSYNCLOCALFILERESPMSG_H_

#include "../SimpleInt64Msg.h"


struct FSyncLocalFileRespMsg;
typedef struct FSyncLocalFileRespMsg FSyncLocalFileRespMsg;

static inline void FSyncLocalFileRespMsg_init(FSyncLocalFileRespMsg* this);
static inline void FSyncLocalFileRespMsg_initFromValue(FSyncLocalFileRespMsg* this,
   int64_t value);

// getters & setters
static inline int64_t FSyncLocalFileRespMsg_getValue(FSyncLocalFileRespMsg* this);


struct FSyncLocalFileRespMsg
{
   SimpleInt64Msg simpleInt64Msg;
};


void FSyncLocalFileRespMsg_init(FSyncLocalFileRespMsg* this)
{
   SimpleInt64Msg_init( (SimpleInt64Msg*)this, NETMSGTYPE_FSyncLocalFileResp);
}

void FSyncLocalFileRespMsg_initFromValue(FSyncLocalFileRespMsg* this, int64_t value)
{
   SimpleInt64Msg_initFromValue( (SimpleInt64Msg*)this, NETMSGTYPE_FSyncLocalFileResp, value);
}

int64_t FSyncLocalFileRespMsg_getValue(FSyncLocalFileRespMsg* this)
{
   return SimpleInt64Msg_getValue( (SimpleInt64Msg*)this);
}


#endif /*FSYNCLOCALFILERESPMSG_H_*/
