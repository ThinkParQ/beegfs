#ifndef BUMPFILEVERSIONMSG_H_
#define BUMPFILEVERSIONMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>

#define BUMPFILEVERSIONMSG_FLAG_PERSISTENT 1 /* change persistent file version number too */
#define BUMPFILEVERSIONMSG_FLAG_HASEVENT   2 /* has a loggable event attached */

/**
 * Note: This message supports only serialization, deserialization is not implemented.
 */
struct BumpFileVersionMsg
{
   NetMessage netMessage;

   const EntryInfo* entryInfo;
   const struct FileEvent* fileEvent;
};
extern const struct NetMessageOps BumpFileVersionMsg_Ops;

static inline void BumpFileVersionMsg_init(struct BumpFileVersionMsg* this,
   const EntryInfo* entryInfo, bool persistent, const struct FileEvent* fileEvent)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_BumpFileVersion, &BumpFileVersionMsg_Ops);

   this->entryInfo = entryInfo;
   this->fileEvent = fileEvent;

   if (persistent)
      this->netMessage.msgHeader.msgFeatureFlags |= BUMPFILEVERSIONMSG_FLAG_PERSISTENT;

   if (fileEvent)
      this->netMessage.msgHeader.msgFeatureFlags |= BUMPFILEVERSIONMSG_FLAG_HASEVENT;
}

#endif
