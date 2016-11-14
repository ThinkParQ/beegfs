#ifndef CLOSEFILEMSGEX_H_
#define CLOSEFILEMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/session/opening/CloseFileMsg.h>
#include <storage/DirInode.h>
#include <storage/FileInode.h>


class CloseFileMsgEx : public CloseFileMsg
{
   public:
      CloseFileMsgEx() : CloseFileMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:

   private:
   
};

#endif /*CLOSEFILEMSGEX_H_*/
