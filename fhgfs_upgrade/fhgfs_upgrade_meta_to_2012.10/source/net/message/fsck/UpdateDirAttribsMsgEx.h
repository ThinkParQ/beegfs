#ifndef UPDATEDIRATTRIBSMSGEX_H
#define UPDATEDIRATTRIBSMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/UpdateDirAttribsMsg.h>
#include <common/net/message/fsck/UpdateDirAttribsRespMsg.h>

class UpdateDirAttribsMsgEx : public UpdateDirAttribsMsg
{
   public:

      UpdateDirAttribsMsgEx() : UpdateDirAttribsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
               char* respBuf, size_t bufLen, HighResolutionStats* stats);
};

#endif /*UPDATEDIRATTRIBSMSGEX_H*/
