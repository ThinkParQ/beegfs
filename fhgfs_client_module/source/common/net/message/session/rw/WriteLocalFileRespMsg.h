#ifndef WRITELOCALFILERESPMSG_H_
#define WRITELOCALFILERESPMSG_H_

#include <common/net/message/SimpleInt64Msg.h>


struct WriteLocalFileRespMsg;
typedef struct WriteLocalFileRespMsg WriteLocalFileRespMsg;

static inline void WriteLocalFileRespMsg_init(WriteLocalFileRespMsg* this);
static inline void WriteLocalFileRespMsg_initFromValue(WriteLocalFileRespMsg* this,
   int64_t value);

// getters & setters
static inline int64_t WriteLocalFileRespMsg_getValue(WriteLocalFileRespMsg* this);

struct WriteLocalFileRespMsg
{
   SimpleInt64Msg simpleInt64Msg;
};


void WriteLocalFileRespMsg_init(WriteLocalFileRespMsg* this)
{
   SimpleInt64Msg_init( (SimpleInt64Msg*)this, NETMSGTYPE_WriteLocalFileResp);
}

void WriteLocalFileRespMsg_initFromValue(WriteLocalFileRespMsg* this, int64_t value)
{
   SimpleInt64Msg_initFromValue( (SimpleInt64Msg*)this, NETMSGTYPE_WriteLocalFileResp, value);
}

int64_t WriteLocalFileRespMsg_getValue(WriteLocalFileRespMsg* this)
{
   return SimpleInt64Msg_getValue( (SimpleInt64Msg*)this);
}


#endif /*WRITELOCALFILERESPMSG_H_*/
