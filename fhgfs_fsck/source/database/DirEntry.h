#ifndef DIRENTRY_H_
#define DIRENTRY_H_

#include <common/fsck/FsckDirEntry.h>
#include <database/EntryID.h>

namespace db {

struct DirEntry
{
   EntryID id;                          /* 12 */
   EntryID parentDirID;                 /* 24 */

   uint32_t entryOwnerNodeID;           /* 28 */
   uint32_t inodeOwnerNodeID;           /* 32 */

   uint32_t saveNodeID;                 /* 36 */
   int32_t saveDevice;                  /* 40 */
   uint64_t saveInode;                  /* 48 */

   union {
      struct {
         char text[16];               /* +16 */
      } inlined;
      struct {
         /* take name from a specific offset in some other file,
          * identified by a u64 for convenience */
         uint64_t fileID;             /* +8 */
         uint64_t fileOffset;         /* +16 */
      } extended;
   } name;                              /* 64 */

   // the identifying key for dir entries is actually (parent, name). since storing the name as
   // part of the key is very inefficient, and existence of duplicate names per parent can be
   // easily excluded, we instead number all dir entries to create a surrogate key (seqNo), which
   // will also work as (parent, seqNo).
   //
   // the actual primary key of the table is (id, parent, GFID, seqNo) though. this is because
   // most querys only need the id, a few queries need the full fsid linking identifier
   // (id, parent, GFID), and deletion always need (parent, seqNo). this also means that the
   // deletion tracker set for dir entries will be rather large per entry, but it does maek queries
   // more efficient.
   uint64_t seqNo;                      /* 72 */

   uint32_t entryType:16;
   uint32_t fileNameInlined:1;
   uint32_t hasInlinedInode:1;
   uint32_t isBuddyMirrored:1;          /* 76 */

   typedef boost::tuple<EntryID, EntryID, uint32_t, int32_t, uint32_t, uint64_t> KeyType;

   KeyType pkey() const
   {
      return KeyType(id, parentDirID, saveNodeID, saveDevice, saveInode, seqNo);
   }

   operator FsckDirEntry() const
   {
      return FsckDirEntry(id.str(), "[<unresolved>]", parentDirID.str(),
         NumNodeID(entryOwnerNodeID), NumNodeID(inodeOwnerNodeID), FsckDirEntryType(entryType),
         hasInlinedInode, NumNodeID(saveNodeID), saveDevice, saveInode, isBuddyMirrored, seqNo);
   }
};

}

#endif
