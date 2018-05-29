#ifndef UPDATEDIRATTRIBSMSGEX_H
#define UPDATEDIRATTRIBSMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/UpdateDirAttribsMsg.h>
#include <common/net/message/fsck/UpdateDirAttribsRespMsg.h>

class UpdateDirAttribsMsgEx : public UpdateDirAttribsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*UPDATEDIRATTRIBSMSGEX_H*/
