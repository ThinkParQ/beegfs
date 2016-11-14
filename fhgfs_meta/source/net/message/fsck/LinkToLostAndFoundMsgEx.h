#ifndef LINKTOLOSTANDFOUNDMSGEX_H
#define LINKTOLOSTANDFOUNDMSGEX_H

#include <common/net/message/NetMessage.h>
#include <common/net/message/fsck/LinkToLostAndFoundMsg.h>
#include <common/net/message/fsck/LinkToLostAndFoundRespMsg.h>
#include <common/net/message/storage/creating/MkDirMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/storage/StorageErrors.h>

#include <dirent.h>

class LinkToLostAndFoundMsgEx : public LinkToLostAndFoundMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      void linkDirInodes(FsckDirInodeList* outFailedInodes, FsckDirEntryList* outCreatedDirEntries);
      void linkFileInodes(FsckFileInodeList* outFailedInodes,
         FsckDirEntryList* outCreatedDirEntries);

      FhgfsOpsErr deleteInode(std::string& entryID, uint16_t ownerNodeID);
};

#endif /*LINKTOLOSTANDFOUNDMSGEX_H*/
