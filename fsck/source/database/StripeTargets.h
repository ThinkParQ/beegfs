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

   friend std::ostream& operator<<(std::ostream& os, StripeTargets const& obj)
   {
      os << "-------------" << "\n";
      os << "StripeTargets" << "\n";
      os << "-------------" << "\n";
      os << "EntryID: " << obj.id.str() << "\n";
      os << "Stripe targets: [ ";

      for (int i=0; i<obj.NTARGETS; i++)
      {
         os <<  obj.targets[i] << " ";
      }
      os << "]\n";

      os << "FirstTargetsIndex: " << obj.firstTargetIndex << "\n";
      return os;
   }
};

}

#endif
