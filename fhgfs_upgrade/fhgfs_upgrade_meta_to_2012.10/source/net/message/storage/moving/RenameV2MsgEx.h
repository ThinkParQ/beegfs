#ifndef RENAMEV2MSGEX_H_
#define RENAMEV2MSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/moving/RenameMsg.h>
#include <storage/MetaStore.h>


class RenameV2MsgEx : public RenameMsg
{
   public:
      RenameV2MsgEx() : RenameMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);
   
   protected:
   
   private:
      FhgfsOpsErr moveFrom(EntryInfo* fromDirInfo, std::string& oldName, DirEntryType entryType,
         EntryInfo* toDirInfo, std::string& newName);
      FhgfsOpsErr movingPerform(DirInode* fromParent, EntryInfo* fromDirInfo,
         std::string& oldName, DirEntryType entryType, EntryInfo* toDirInfo, std::string& newName);

      bool checkParentToSubdir(std::string fromPathStr, std::string toPathStr);
      bool checkRenameSimple(EntryInfo* fromDirInfo, EntryInfo* toDirInfo);
      
      FhgfsOpsErr renameInSameDir(DirInode* fromParent, std::string oldName, std::string newName);
      FhgfsOpsErr renameDir(DirInode* fromParent, EntryInfo* fromDirInfo,
         std::string& oldName, EntryInfo* toDirInfo, std::string& newName);
      FhgfsOpsErr renameFile(DirInode* fromParent, EntryInfo* fromDirInfo,
         std::string& oldName, EntryInfo* toDirInfo, std::string& newName);
      
      FhgfsOpsErr remoteFileInsertAndUnlink(EntryInfo* fromFileInfo, EntryInfo* toDirInfo,
         std::string newName, char* serialBuf, size_t serialBufLen);
      FhgfsOpsErr remoteDirInsert(EntryInfo* toDirInfo, std::string& newName,
         char* serialBuf, size_t serialBufLen);
};


#endif /*RENAMEV2MSGEX_H_*/
