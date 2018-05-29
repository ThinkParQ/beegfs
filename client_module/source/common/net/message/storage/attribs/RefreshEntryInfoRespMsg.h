#ifndef REFRESHENTRYINFORESPMSG_H_
#define REFRESHENTRYINFORESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct RefreshEntryInfoRespMsg;
typedef struct RefreshEntryInfoRespMsg RefreshEntryInfoRespMsg;

static inline void RefreshEntryInfoRespMsg_init(RefreshEntryInfoRespMsg* this);

// getters & setters
static inline int RefreshEntryInfoRespMsg_getResult(RefreshEntryInfoRespMsg* this);

struct RefreshEntryInfoRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void RefreshEntryInfoRespMsg_init(RefreshEntryInfoRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_RefreshEntryInfoResp);
}

int RefreshEntryInfoRespMsg_getResult(RefreshEntryInfoRespMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}

#endif /*REFRESHENTRYINFORESPMSG_H_*/
