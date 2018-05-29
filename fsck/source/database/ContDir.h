#ifndef CONTDIR_H_
#define CONTDIR_H_

#include <common/fsck/FsckContDir.h>
#include <database/EntryID.h>

namespace db {

struct ContDir {
   EntryID id;                 /* 12 */
   uint32_t saveNodeID;        /* 16 */
   uint32_t isBuddyMirrored:1; /* 20 */

   typedef EntryID KeyType;

   KeyType pkey() const { return id; }

   operator FsckContDir() const
   {
      return FsckContDir(id.str(), NumNodeID(saveNodeID), isBuddyMirrored);
   }
};

}

#endif
