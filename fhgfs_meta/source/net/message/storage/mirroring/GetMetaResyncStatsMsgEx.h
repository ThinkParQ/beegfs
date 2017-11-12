#ifndef META_GETMETARESYNCJOBSTATSMSGEX_H
#define META_GETMETARESYNCJOBSTATSMSGEX_H

#include <common/net/message/storage/mirroring/GetMetaResyncStatsMsg.h>

class GetMetaResyncStatsMsgEx : public GetMetaResyncStatsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* META_GETMETARESYNCJOBSTATSMSGEX_H */
