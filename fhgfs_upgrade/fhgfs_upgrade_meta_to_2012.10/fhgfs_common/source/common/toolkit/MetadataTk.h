#ifndef METADATATK_H_
#define METADATATK_H_

#include <common/nodes/NodeStoreServers.h>
#include <common/storage/Metadata.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/EntryOwnerInfo.h>
#include <common/toolkit/MessagingTk.h>
#include <common/Common.h>
#include <common/storage/striping/StripePattern.h>


#define METADATATK_OWNERSEARCH_MAX_STEPS    128


class MetadataTk
{
   public:
      static FhgfsOpsErr referenceOwner(Path* searchPath, bool referenceParent,
         NodeStoreServers* nodes, Node** outReferencedNode, EntryInfo* entryInfo);
   
   private:
      MetadataTk() {}
   
      static FhgfsOpsErr findOwnerStep(Path* searchPath, Node* node,
         NetMessage* requestMsg, EntryOwnerInfo* outInfo);

      static FhgfsOpsErr findOwner(Path* searchPath, unsigned searchDepth, NodeStoreServers* nodes,
         EntryInfo* outEntryInfo);


   public:
      // inliners
      static DirEntryType posixFileTypeToDirEntryType(unsigned posixFileMode)
      {
         if(S_ISDIR(posixFileMode) )
            return DirEntryType_DIRECTORY;

         if(S_ISREG(posixFileMode) )
            return DirEntryType_REGULARFILE;

         if(S_ISLNK(posixFileMode) )
            return DirEntryType_SYMLINK;

         if(S_ISBLK(posixFileMode) )
            return DirEntryType_BLOCKDEV;

         if(S_ISCHR(posixFileMode) )
            return DirEntryType_CHARDEV;

         if(S_ISFIFO(posixFileMode) )
            return DirEntryType_FIFO;

         if(S_ISSOCK(posixFileMode) )
            return DirEntryType_SOCKET;

         return DirEntryType_INVALID;
      }
};

#endif /*METADATATK_H_*/
