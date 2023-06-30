#ifndef FILEINODE_H_
#define FILEINODE_H_

#include <common/fsck/FsckFileInode.h>
#include <database/EntryID.h>

namespace db {

struct FileInode {
   EntryID id;                   /* 12 */
   EntryID parentDirID;          /* 24 */
   EntryID origParentEntryID;    /* 36 */

   uint32_t parentNodeID;        /* 40 */
   uint32_t saveNodeID;          /* 44 */
   uint32_t origParentUID;       /* 48 */

   uint32_t uid;                 /* 52 */
   uint32_t gid;                 /* 56 */

   uint64_t fileSize;            /* 64 */
   uint64_t usedBlocks;          /* 72 */

   uint64_t numHardlinks;        /* 80 */

   uint64_t saveInode;           /* 88 */
   int32_t saveDevice;           /* 92 */

   uint32_t chunkSize;           /* 96 */

   enum {
      NTARGETS = 6
   };
   uint16_t targets[NTARGETS];   /* 108 */

   uint32_t isInlined:1;
   uint32_t pathInfoFlags:4;
   uint32_t stripePatternType:4;
   uint32_t stripePatternSize:20;
   uint32_t readable:1;
   uint32_t isBuddyMirrored:1;
   uint32_t isMismirrored:1;     /* 112 */

   typedef EntryID KeyType;

   EntryID pkey() const { return id; }

   FsckFileInode toInodeWithoutStripes() const
   {
      PathInfo info(origParentUID, origParentEntryID.str(), pathInfoFlags);

      return FsckFileInode(id.str(), parentDirID.str(), NumNodeID(parentNodeID), info, uid, gid,
            fileSize, numHardlinks, usedBlocks, {}, FsckStripePatternType(stripePatternType),
            chunkSize, NumNodeID(saveNodeID), saveInode, saveDevice, isInlined, isBuddyMirrored,
            readable, isMismirrored);
   }

   friend std::ostream& operator<<(std::ostream& os, FileInode const& obj)
   {
         os << "-------------" << "\n";
         os << "FsckFileInode" << "\n";
         os << "-------------" << "\n";
         os << "EntryID: " << obj.id.str() << "\n";
         os << "Parent dir's entryID: " << obj.parentDirID.str() << "\n";
         os << "Orig parent dir's entryID: " << obj.origParentEntryID.str() << "\n";
         os << "parent nodeID: " << obj.parentNodeID << "\n";
         os << "save nodeID: " << obj.saveNodeID << "\n";
         os << "origParentUID: " << obj.origParentUID << "\n";
         os << "uid: " << obj.uid << "\n";
         os << "gid: " << obj.gid << "\n";
         os << "fileSize: " << obj.fileSize << "\n";
         os << "usedBlocks: " << obj.usedBlocks << "\n";
         os << "numHardlinks: " << obj.numHardlinks << "\n";
         os << "saveInode: " << obj.saveInode << "\n";
         os << "saveDevice: " << obj.saveDevice << "\n";
         os << "chunkSize: " << obj.chunkSize << "\n";

         os << "Stripe targets: [ ";

         for (int i=0; i< NTARGETS; i++)
         {
            os << obj.targets[i] << " ";
         }
         os << "]\n";

         os << "isInlined: " << obj.isInlined << "\n";
         os << "pathInfoFlags: " << obj.pathInfoFlags << "\n";
         os << "stripePatternType: " << obj.stripePatternType << "\n";
         os << "stripePatternSize: " << obj.stripePatternSize << "\n";
         os << "readable: " << obj.readable << "\n";
         os << "isBuddyMirrored: " << obj.isBuddyMirrored << "\n";
         os << "isMismirrored: " << obj.isMismirrored << "\n\n";
         return os;
   }
};

}

#endif
