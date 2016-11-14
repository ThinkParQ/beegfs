#ifndef UPDATEFILEATTRIBSMSGEX_H
#define UPDATEFILEATTRIBSMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/UpdateFileAttribsMsg.h>
#include <common/net/message/fsck/UpdateFileAttribsRespMsg.h>

class UpdateFileAttribsMsgEx : public UpdateFileAttribsMsg
{
   public:

      UpdateFileAttribsMsgEx() : UpdateFileAttribsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
               char* respBuf, size_t bufLen, HighResolutionStats* stats);
};

#endif /*UPDATEFILEATTRIBSMSGEX_H*/
