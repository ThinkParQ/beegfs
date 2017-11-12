#ifndef FIXMETADATAOWNERMSGEX_H
#define FIXMETADATAOWNERMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/DeleteDirEntriesMsg.h>
#include <common/net/message/fsck/DeleteDirEntriesRespMsg.h>

class DeleteDirEntriesMsgEx : public DeleteDirEntriesMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /*FIXMETADATAOWNERMSGEX_H*/
