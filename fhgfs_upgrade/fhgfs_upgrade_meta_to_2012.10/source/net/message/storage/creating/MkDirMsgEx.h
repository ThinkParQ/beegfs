#ifndef MKDIRMSGEX_H_
#define MKDIRMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MkDirMsg.h>
#include <storage/MetaStore.h>


class MkDirMsgEx : public MkDirMsg
{
   public:
      MkDirMsgEx() : MkDirMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

   protected:

   private:
      FhgfsOpsErr mkDir(EntryInfo* parentInfo, std::string newName,
         EntryInfo* outEntryInfo);

      FhgfsOpsErr mkDirDentry(DirInode* parentDir, std::string& name, EntryInfo* entryInfo);
      FhgfsOpsErr mkRemoteDirInode(DirInode* parentDir, std::string& name, EntryInfo* entryInfo);
      FhgfsOpsErr mkRemoteDirCompensate(EntryInfo* entryInfo);

};

#endif /*MKDIRMSGEX_H_*/
