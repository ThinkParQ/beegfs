#ifndef OPENFILEMSGEX_H_
#define OPENFILEMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/session/opening/OpenFileMsg.h>
#include <storage/DirInode.h>
#include <storage/FileInode.h>


class OpenFileMsgEx : public OpenFileMsg
{
   public:
      OpenFileMsgEx() : OpenFileMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

     
   protected:


   private:
      FhgfsOpsErr openMetaFile(std::string fileID, FileInode** outOpenFile);
      void openMetaFileCompensate(FileInode* openFile);
};
   
#endif /*OPENFILEMSGEX_H_*/
