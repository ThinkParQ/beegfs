#ifndef WRITELOCALFILERDMARESPMSG_H_
#define WRITELOCALFILERDMARESPMSG_H_
#ifdef BEEGFS_NVFS

#include <common/net/message/SimpleInt64Msg.h>


struct WriteLocalFileRDMARespMsg;
typedef struct WriteLocalFileRDMARespMsg WriteLocalFileRDMARespMsg;

static inline void WriteLocalFileRDMARespMsg_init(WriteLocalFileRDMARespMsg* this);

// getters & setters
static inline int64_t WriteLocalFileRDMARespMsg_getValue(WriteLocalFileRDMARespMsg* this);

struct WriteLocalFileRDMARespMsg
{
   SimpleInt64Msg simpleInt64Msg;
};


void WriteLocalFileRDMARespMsg_init(WriteLocalFileRDMARespMsg* this)
{
   SimpleInt64Msg_init( (SimpleInt64Msg*)this, NETMSGTYPE_WriteLocalFileRDMAResp);
}

int64_t WriteLocalFileRDMARespMsg_getValue(WriteLocalFileRDMARespMsg* this)
{
   return SimpleInt64Msg_getValue( (SimpleInt64Msg*)this);
}


#endif /* BEEGFS_NVFS */
#endif /*WRITELOCALFILERDMARESPMSG_H_*/
