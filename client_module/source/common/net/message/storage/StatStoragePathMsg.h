#ifndef STATSTORAGEPATHMSG_H_
#define STATSTORAGEPATHMSG_H_

#include "../SimpleUInt16Msg.h"


struct StatStoragePathMsg;
typedef struct StatStoragePathMsg StatStoragePathMsg;

static inline void StatStoragePathMsg_init(StatStoragePathMsg* this);
static inline void StatStoragePathMsg_initFromTarget(StatStoragePathMsg* this,
   uint16_t targetID);

struct StatStoragePathMsg
{
   SimpleUInt16Msg simpleUInt16Msg;
};


void StatStoragePathMsg_init(StatStoragePathMsg* this)
{
   SimpleUInt16Msg_init( (SimpleUInt16Msg*)this, NETMSGTYPE_StatStoragePath);
}

/**
 * @param targetID only used for storage servers, ignored for other nodes (but may not be NULL!)
 */
void StatStoragePathMsg_initFromTarget(StatStoragePathMsg* this, uint16_t targetID)
{
   SimpleUInt16Msg_initFromValue( (SimpleUInt16Msg*)this, NETMSGTYPE_StatStoragePath, targetID);
}

#endif /*STATSTORAGEPATHMSG_H_*/
