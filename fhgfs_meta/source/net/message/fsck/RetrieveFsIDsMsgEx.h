#ifndef RETRIEVEFSIDSMSGEX_H
#define RETRIEVEFSIDSMSGEX_H

#include <common/net/message/fsck/RetrieveFsIDsMsg.h>
#include <common/net/message/fsck/RetrieveFsIDsRespMsg.h>

class RetrieveFsIDsMsgEx : public RetrieveFsIDsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*RETRIEVEFSIDSMSGEX_H*/
