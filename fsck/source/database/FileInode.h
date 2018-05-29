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
};

}

#endif
