#ifndef SETATTRMSGEX_H_
#define SETATTRMSGEX_H_

#include <storage/MetaStore.h>
#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/attribs/SetAttrMsg.h>

class SetAttrMsgEx : public SetAttrMsg
{
   public:
      SetAttrMsgEx() : SetAttrMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr setAttr(EntryInfo* entryInfo);
      FhgfsOpsErr setAttrRoot();
      FhgfsOpsErr setChunkFileAttribs(FileInode* file);
      FhgfsOpsErr setChunkFileAttribsSequential(FileInode* file);
      FhgfsOpsErr setChunkFileAttribsParallel(FileInode* file);
};


#endif /*SETATTRMSGEX_H_*/
