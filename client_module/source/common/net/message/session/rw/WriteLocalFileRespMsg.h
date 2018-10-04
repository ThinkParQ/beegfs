#ifndef WRITELOCALFILERESPMSG_H_
#define WRITELOCALFILERESPMSG_H_

#include <common/net/message/SimpleInt64Msg.h>


struct WriteLocalFileRespMsg;
typedef struct WriteLocalFileRespMsg WriteLocalFileRespMsg;

static inline void WriteLocalFileRespMsg_init(WriteLocalFileRespMsg* this);

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

int64_t WriteLocalFileRespMsg_getValue(WriteLocalFileRespMsg* this)
{
   return SimpleInt64Msg_getValue( (SimpleInt64Msg*)this);
}


#endif /*WRITELOCALFILERESPMSG_H_*/
