#pragma once

#include <common/storage/striping/Raid0Pattern.h>
#include <common/net/message/fsck/CreateDefDirInodesMsg.h>
#include <common/net/message/fsck/CreateDefDirInodesRespMsg.h>

class CreateDefDirInodesMsgEx : public CreateDefDirInodesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

