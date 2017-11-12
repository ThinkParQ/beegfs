#ifndef MSGHELPEROPEN_H_
#define MSGHELPEROPEN_H_

#include <common/Common.h>
#include <common/storage/EntryInfo.h>
#include <storage/MetaStore.h>
#include <storage/MetadataEx.h>


class MsgHelperOpen
{
   public:
      static FhgfsOpsErr openFile(EntryInfo* entryInfo, unsigned accessFlags,
         FileInode** outOpenFile);


   private:
      MsgHelperOpen() {}

      static FhgfsOpsErr openMetaFile(EntryInfo* entryInfo,
         unsigned accessFlags, FileInode** outOpenFile);
      static void openMetaFileCompensate(EntryInfo* entryInfo,
         FileInode* inode, unsigned accessFlags);

};

#endif /* MSGHELPEROPEN_H_ */
