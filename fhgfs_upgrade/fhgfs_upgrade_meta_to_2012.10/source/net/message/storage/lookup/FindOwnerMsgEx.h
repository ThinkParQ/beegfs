#ifndef FINDOWNERMSGEX_H_
#define FINDOWNERMSGEX_H_

#include <common/net/message/storage/lookup/FindOwnerMsg.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MetadataTk.h>
#include <common/Common.h>
#include <storage/MetaStore.h>

// Get the meta-data server of a file

class FindOwnerMsgEx : public FindOwnerMsg
{
   public:
      FindOwnerMsgEx() : FindOwnerMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr findOwnerRec(DirInode* dir, StringListIter pathIter,
         unsigned currentDepth, unsigned numPathElems, EntryOwnerInfo* outInfo);
      FhgfsOpsErr findOwner(EntryOwnerInfo* outInfo);
   
};


#endif /*FINDOWNERMSGEX_H_*/
