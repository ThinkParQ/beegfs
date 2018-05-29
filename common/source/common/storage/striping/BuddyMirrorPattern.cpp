#include "BuddyMirrorPattern.h"

bool BuddyMirrorPattern::updateStripeTargetIDs(StripePattern* updatedStripePattern)
{
   if ( !updatedStripePattern )
      return false;

   if ( this->mirrorBuddyGroupIDs.size() != updatedStripePattern->getStripeTargetIDs()->size() )
      return false;

   for ( unsigned i = 0; i < mirrorBuddyGroupIDs.size(); i++ )
   {
      mirrorBuddyGroupIDs[i] = updatedStripePattern->getStripeTargetIDs()->at(i);
   }

   return true;
}

bool BuddyMirrorPattern::patternEquals(const StripePattern* second) const
{
   const BuddyMirrorPattern* other = (const BuddyMirrorPattern*) second;

   return defaultNumTargets == other->defaultNumTargets
      && mirrorBuddyGroupIDs == other->mirrorBuddyGroupIDs;
}
