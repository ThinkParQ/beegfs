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
         std::unique_ptr<FileInode>* outUnlinkedFile);
      static FhgfsOpsErr unlinkFileInode(EntryInfo* delFileInfo,
         std::unique_ptr<FileInode>* outUnlinkedFile);
      static FhgfsOpsErr unlinkChunkFiles(FileInode* unlinkedInode, unsigned msgUserID);

   private:
      MsgHelperUnlink() {}

      static FhgfsOpsErr unlinkChunkFileSequential(FileInode& inode, unsigned msgUserID);
      static FhgfsOpsErr unlinkChunkFileParallel(FileInode& inode, unsigned msgUserID);
      static FhgfsOpsErr insertDisposableFile(FileInode& file);


   public:
      // inliners

      static FhgfsOpsErr unlinkChunkFilesInternal(FileInode& file, unsigned msgUserID);
};

#endif /* MSGHELPERUNLINK_H_ */
