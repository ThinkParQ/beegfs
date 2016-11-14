#ifndef MSGHELPERUNLINK_H_
#define MSGHELPERUNLINK_H_

#include <common/storage/Path.h>
#include <common/Common.h>
#include <storage/MetaStore.h>


class MsgHelperUnlink
{
   public:
      static FhgfsOpsErr unlinkFile(std::string parentID, std::string removeName);
      static FhgfsOpsErr unlinkChunkFiles(FileInode* inode);
      
   private:
      MsgHelperUnlink() {}
      
      static FhgfsOpsErr unlinkLocalFileSequential(FileInode* file);
      static FhgfsOpsErr unlinkLocalFileParallel(FileInode* file);
      static FhgfsOpsErr insertDisposableFile(FileInode* file);
      
      
   public:
      // inliners

      static FhgfsOpsErr unlinkChunkFilesInternal(FileInode* file);
};

#endif /* MSGHELPERUNLINK_H_ */
