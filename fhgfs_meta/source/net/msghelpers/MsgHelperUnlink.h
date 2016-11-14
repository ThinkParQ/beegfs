#ifndef MSGHELPERUNLINK_H_
#define MSGHELPERUNLINK_H_

#include <common/storage/Path.h>
#include <common/Common.h>
#include <storage/MetaStore.h>


class MsgHelperUnlink
{
   public:
      static FhgfsOpsErr unlinkFile(DirInode& parentDir, const std::string& removeName,
         unsigned msgUserID);
      static FhgfsOpsErr unlinkMetaFile(DirInode& parentDir, const std::string& removeName,
         FileInode** outUnlinkedFile);
      static FhgfsOpsErr unlinkChunkFiles(FileInode* inode, unsigned msgUserID);

   private:
      MsgHelperUnlink() {}

      static FhgfsOpsErr unlinkChunkFileSequential(FileInode& file, unsigned msgUserID);
      static FhgfsOpsErr unlinkChunkFileParallel(FileInode& file, unsigned msgUserID);
      static FhgfsOpsErr insertDisposableFile(FileInode& file);


   public:
      // inliners

      static FhgfsOpsErr unlinkChunkFilesInternal(FileInode& file, unsigned msgUserID);
};

#endif /* MSGHELPERUNLINK_H_ */
