#ifndef CREATEEMPTYCONTDIRSMSGEX_H
#define CREATEEMPTYCONTDIRSMSGEX_H

#include <common/storage/striping/Raid0Pattern.h>
#include <common/net/message/fsck/CreateEmptyContDirsMsg.h>
#include <common/net/message/fsck/CreateEmptyContDirsRespMsg.h>

class CreateEmptyContDirsMsgEx : public CreateEmptyContDirsMsg
{
   public:
      CreateEmptyContDirsMsgEx() : CreateEmptyContDirsMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
               char* respBuf, size_t bufLen, HighResolutionStats* stats);

   private:

};

#endif /*CREATEEMPTYCONTDIRSMSGEX_H*/
