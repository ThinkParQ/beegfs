#ifndef MKFILEWITHPATTERNMSGEX_H_
#define MKFILEWITHPATTERNMSGEX_H_

#include <common/storage/StorageErrors.h>
#include <common/net/message/storage/creating/MkFileWithPatternMsg.h>
#include <storage/MetaStore.h>

/**
 * Similar to class MsgHelperMkFile, but called with a create pattern, for example from fhgfs-ctl
 * or from ioctl calls.
 */
class MkFileWithPatternMsgEx : public MkFileWithPatternMsg
{
   public:
      MkFileWithPatternMsgEx() : MkFileWithPatternMsg()
      {
      }

      virtual bool processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
         char* respBuf, size_t bufLen, HighResolutionStats* stats);

   protected:

   private:
      FhgfsOpsErr mkFile(EntryInfo* parentInfo, std::string newFileName,
         EntryInfo* outEntryInfo);
      FhgfsOpsErr mkMetaFile(DirInode* dir, std::string filename, EntryInfo* outEntryInfo);

};

#endif /* MKFILEWITHPATTERNMSGEX_H_ */
