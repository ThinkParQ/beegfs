#ifndef MKFILEMSGEX_H_
#define MKFILEMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MkFileMsg.h>
#include <storage/MetaStore.h>


class MkFileMsgEx : public MkFileMsg
{
   public:
      MkFileMsgEx() : MkFileMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

   protected:

   private:

};

#endif /*MKFILEMSGEX_H_*/
