#ifndef BUMPFILEVERSIONMSG_H_
#define BUMPFILEVERSIONMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

/**
 * Note: This message supports only serialization, deserialization is not implemented.
 */
struct BumpFileVersionMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfo;
};
extern const struct NetMessageOps BumpFileVersionMsg_Ops;

static inline void BumpFileVersionMsg_init(struct BumpFileVersionMsg* this,
   const EntryInfo* entryInfo)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_BumpFileVersion, &BumpFileVersionMsg_Ops);

   this->entryInfo = entryInfo;
}

#endif
