#ifndef FINDLINKOWNERMSGEX_H
#define FINDLINKOWNERMSGEX_H

#include <common/net/message/storage/lookup/FindLinkOwnerMsg.h>
#include <common/net/message/storage/lookup/FindLinkOwnerRespMsg.h>

class FindLinkOwnerMsgEx : public FindLinkOwnerMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*FINDLINKOWNERMSGEX_H*/
