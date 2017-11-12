#ifndef CHUNK_H_
#define CHUNK_H_

#include <common/fsck/FsckChunk.h>
#include <database/EntryID.h>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

namespace db {

struct Chunk {
   static const unsigned SAVED_PATH_SIZE = 43;

   EntryID id;             /* 12 */
   uint32_t targetID;      /* 16 */
   uint32_t buddyGroupID;  /* 20 */

   /* savedPath layout is ... weird.
    * we have two layouts:
    *   - <u16 _b16>/<u16 _b16>/<id _string>
    *   - u<origOwnerUID _b16>/<(ts >> 16) _b16>/<(ts >> 12) & 0xF _b16>/<id _string>
    *
    * where u16 are random 16 bit unsigneds each. that gives maximum path lengths of
    *   - 2 + 1 + 2 + 26
    *   - 9 + 1 + 4 + 1 + 1 + 1 + 26
    * add a terminating NUL to each, the maximum path length becomes 44 bytes,
    * which is also conveniently aligned for uint_64t followers */
   char savedPath[SAVED_PATH_SIZE + 1]; /* 64 */

   int64_t fileSize;       /* 72 */
   int64_t usedBlocks;     /* 80 */

   uint32_t uid;           /* 84 */
   uint32_t gid;           /* 88 */

   typedef boost::tuple<EntryID, uint32_t, uint32_t> KeyType;

   KeyType pkey() const
   {
      return KeyType(id, targetID, buddyGroupID);
   }

   operator FsckChunk() const
   {
      Path path(savedPath);

      return FsckChunk(id.str(), targetID, path, fileSize, usedBlocks, 0, 0, 0, uid,
         gid, buddyGroupID);
   }
};

}

#endif
