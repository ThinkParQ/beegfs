#ifndef UPDATEFILEATTRIBSMSGEX_H
#define UPDATEFILEATTRIBSMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/UpdateFileAttribsMsg.h>
#include <common/net/message/fsck/UpdateFileAttribsRespMsg.h>

class UpdateFileAttribsMsgEx : public UpdateFileAttribsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*UPDATEFILEATTRIBSMSGEX_H*/
