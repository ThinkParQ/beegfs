#ifndef CREATEDEFDIRINODESMSGEX_H
#define CREATEDEFDIRINODESMSGEX_H

#include <common/storage/striping/Raid0Pattern.h>
#include <common/net/message/fsck/CreateDefDirInodesMsg.h>
#include <common/net/message/fsck/CreateDefDirInodesRespMsg.h>

class CreateDefDirInodesMsgEx : public CreateDefDirInodesMsg
{
   public:
      CreateDefDirInodesMsgEx() : CreateDefDirInodesMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
               char* respBuf, size_t bufLen, HighResolutionStats* stats);

   private:

};

#endif /*CREATEDEFDIRINODESMSGEX_H*/
