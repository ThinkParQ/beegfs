#ifndef GETHIGHRESSTATSMSGEX_H_
#define GETHIGHRESSTATSMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/GetHighResStatsMsg.h>


class GetHighResStatsMsgEx : public GetHighResStatsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* GETHIGHRESSTATSMSGEX_H_ */
