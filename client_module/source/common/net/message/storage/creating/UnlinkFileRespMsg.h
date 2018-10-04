#ifndef UNLINKFILERESPMSG_H_
#define UNLINKFILERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct UnlinkFileRespMsg;
typedef struct UnlinkFileRespMsg UnlinkFileRespMsg;

static inline void UnlinkFileRespMsg_init(UnlinkFileRespMsg* this);

// getters & setters
static inline int UnlinkFileRespMsg_getValue(UnlinkFileRespMsg* this);

struct UnlinkFileRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void UnlinkFileRespMsg_init(UnlinkFileRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_UnlinkFileResp);
}

int UnlinkFileRespMsg_getValue(UnlinkFileRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}



#endif /*UNLINKFILERESPMSG_H_*/
