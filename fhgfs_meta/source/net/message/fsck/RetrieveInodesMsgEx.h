#ifndef RETRIEVEINODESMSGEX_H
#define RETRIEVEINODESMSGEX_H

#include <common/net/message/fsck/RetrieveInodesMsg.h>
#include <common/net/message/fsck/RetrieveInodesRespMsg.h>

class RetrieveInodesMsgEx : public RetrieveInodesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*RETRIEVEINODESMSGEX_H*/
