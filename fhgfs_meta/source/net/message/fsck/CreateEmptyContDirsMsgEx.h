#ifndef CREATEEMPTYCONTDIRSMSGEX_H
#define CREATEEMPTYCONTDIRSMSGEX_H

#include <common/storage/striping/Raid0Pattern.h>
#include <common/net/message/fsck/CreateEmptyContDirsMsg.h>
#include <common/net/message/fsck/CreateEmptyContDirsRespMsg.h>

class CreateEmptyContDirsMsgEx : public CreateEmptyContDirsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*CREATEEMPTYCONTDIRSMSGEX_H*/
