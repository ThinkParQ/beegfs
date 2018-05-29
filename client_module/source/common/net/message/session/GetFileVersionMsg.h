#ifndef GETFILEVERSIONMSG_H_
#define GETFILEVERSIONMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

/**
 * Note: This message supports only serialization, deserialization is not implemented.
 */
struct GetFileVersionMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfo;
};
extern const struct NetMessageOps GetFileVersionMsg_Ops;

static inline void GetFileVersionMsg_init(struct GetFileVersionMsg* this,
   const EntryInfo* entryInfo)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetFileVersion, &GetFileVersionMsg_Ops);

   this->entryInfo = entryInfo;
}

#endif
