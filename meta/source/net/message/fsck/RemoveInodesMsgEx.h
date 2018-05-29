#ifndef REMOVEINODESMSGEX_H
#define REMOVEINODESMSGEX_H

#include <common/net/message/fsck/RemoveInodesMsg.h>

class RemoveInodesMsgEx : public RemoveInodesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*REMOVEINODESMSGEX_H*/
