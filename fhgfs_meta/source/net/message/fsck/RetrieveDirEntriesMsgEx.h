#ifndef RETRIEVEDIRENTRIESMSGEX_H
#define RETRIEVEDIRENTRIESMSGEX_H

#include <common/net/message/fsck/RetrieveDirEntriesMsg.h>
#include <common/net/message/fsck/RetrieveDirEntriesRespMsg.h>

class RetrieveDirEntriesMsgEx : public RetrieveDirEntriesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*RETRIEVEDIRENTRIESMSGEX_H*/
