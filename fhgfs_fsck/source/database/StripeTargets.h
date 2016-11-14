#ifndef STRIPETARGETS_H_
#define STRIPETARGETS_H_

#include <database/EntryID.h>

#include <inttypes.h>

namespace db {

struct StripeTargets
{
   EntryID id;                 /* 12 */

   enum {
      NTARGETS = 7
   };
   uint16_t targets[NTARGETS]; /* 26 */
   uint16_t firstTargetIndex;  /* 28 */

   typedef EntryID KeyType;

   EntryID pkey() const { return id; }
};

}

#endif
