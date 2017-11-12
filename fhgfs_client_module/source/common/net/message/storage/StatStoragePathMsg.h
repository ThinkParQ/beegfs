#ifndef STATSTORAGEPATHMSG_H_
#define STATSTORAGEPATHMSG_H_

#include "../SimpleUInt16Msg.h"


struct StatStoragePathMsg;
typedef struct StatStoragePathMsg StatStoragePathMsg;

static inline void StatStoragePathMsg_init(StatStoragePathMsg* this);
static inline void StatStoragePathMsg_initFromTarget(StatStoragePathMsg* this,
   uint16_t targetID);

// getters & setters
static inline uint16_t StatStoragePathMsg_getTargetID(StatStoragePathMsg* this);


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

uint16_t StatStoragePathMsg_getTargetID(StatStoragePathMsg* this)
{
   return SimpleUInt16Msg_getValue( (SimpleUInt16Msg*)this);
}

#endif /*STATSTORAGEPATHMSG_H_*/
