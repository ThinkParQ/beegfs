#ifndef GETCLIENTSTATSMSGEX_H
#define GETCLIENTSTATSMSGEX_H

#include <common/storage/StorageErrors.h>
#include <common/net/message/nodes/GetClientStatsMsg.h>


// NOTE: The message factory requires this object to have 'deserialize' and
//       'processIncoming' methods. 'deserialize' is derived from other classes.

class GetClientStatsMsgEx : public GetClientStatsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* GETCLIENTSTATSMSGEX_H */
