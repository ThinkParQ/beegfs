#ifndef GETCLIENTSTATSMSGEX_H
#define GETCLIENTSTATSMSGEX_H

#include <common/storage/StorageErrors.h>
#include <common/net/message/nodes/GetClientStatsMsg.h>


class GetClientStatsMsgEx : public GetClientStatsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* GETCLIENTSTATSMSGEX_H */
