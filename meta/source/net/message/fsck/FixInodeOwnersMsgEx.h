#ifndef FIXINODEOWNERSMSGEX_H
#define FIXINODEOWNERSMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/FixInodeOwnersMsg.h>
#include <common/net/message/fsck/FixInodeOwnersRespMsg.h>

class FixInodeOwnersMsgEx : public FixInodeOwnersMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*FIXINODEOWNERSMSGEX_H*/
