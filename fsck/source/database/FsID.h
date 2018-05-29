#ifndef FSID_H_
#define FSID_H_

#include <common/fsck/FsckFsID.h>
#include <database/EntryID.h>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

namespace db {

struct FsID {
   EntryID id;                  /* 12 */
   EntryID parentDirID;         /* 24 */

   uint32_t saveNodeID;         /* 28 */
   int32_t saveDevice;          /* 32 */
   uint64_t saveInode;          /* 40 */

   uint64_t isBuddyMirrored:1;  /* 48 */

   typedef boost::tuple<EntryID, EntryID, uint32_t, int32_t, uint32_t> KeyType;

   KeyType pkey() const
   {
      return KeyType(id, parentDirID, saveNodeID, saveDevice, saveInode);
   }

   operator FsckFsID() const
   {
      return FsckFsID(id.str(), parentDirID.str(), NumNodeID(saveNodeID), saveDevice, saveInode,
            isBuddyMirrored);
   }
};

}

#endif
