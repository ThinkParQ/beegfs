#ifndef RENAMERESPMSG_H_
#define RENAMERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct RenameRespMsg;
typedef struct RenameRespMsg RenameRespMsg;

static inline void RenameRespMsg_init(RenameRespMsg* this);

// getters & setters
static inline int RenameRespMsg_getValue(RenameRespMsg* this);

struct RenameRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void RenameRespMsg_init(RenameRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_RenameResp);
}

int RenameRespMsg_getValue(RenameRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}


#endif /*RENAMERESPMSG_H_*/
