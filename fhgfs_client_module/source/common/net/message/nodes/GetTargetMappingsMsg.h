#ifndef GETTARGETMAPPINGSMSG_H_
#define GETTARGETMAPPINGSMSG_H_

#include "../SimpleMsg.h"


struct GetTargetMappingsMsg;
typedef struct GetTargetMappingsMsg GetTargetMappingsMsg;

static inline void GetTargetMappingsMsg_init(GetTargetMappingsMsg* this);


struct GetTargetMappingsMsg
{
   SimpleMsg simpleMsg;
};


void GetTargetMappingsMsg_init(GetTargetMappingsMsg* this)
{
   SimpleMsg_init( (SimpleMsg*)this, NETMSGTYPE_GetTargetMappings);
}

#endif /* GETTARGETMAPPINGSMSG_H_ */
