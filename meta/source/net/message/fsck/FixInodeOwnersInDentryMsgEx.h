#ifndef FIXINODEOWNERSINDENTRYMSGEX_H
#define FIXINODEOWNERSINDENTRYMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/FixInodeOwnersInDentryMsg.h>
#include <common/net/message/fsck/FixInodeOwnersInDentryRespMsg.h>

class FixInodeOwnersInDentryMsgEx : public FixInodeOwnersInDentryMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*FIXINODEOWNERSINDENTRYMSGEX_H*/
