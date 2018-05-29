#ifndef FSCKMODIFICATIONEVENTMSGEX_H
#define FSCKMODIFICATIONEVENTMSGEX_H

#include <common/net/message/fsck/FsckModificationEventMsg.h>

class FsckModificationEventMsgEx : public FsckModificationEventMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*FSCKMODIFICATIONEVENTMSGEX_H*/
