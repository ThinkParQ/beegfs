#include "Raid10Pattern.h"

bool Raid10Pattern::updateStripeTargetIDs(StripePattern* updatedStripePattern)
{
   if ( !updatedStripePattern )
      return false;

   if ( this->stripeTargetIDs.size() != updatedStripePattern->getStripeTargetIDs()->size() )
      return false;

   for ( unsigned i = 0; i < stripeTargetIDs.size(); i++ )
   {
      stripeTargetIDs[i] = updatedStripePattern->getStripeTargetIDs()->at(i);
   }

   return true;
}

bool Raid10Pattern::patternEquals(const StripePattern* second) const
{
   const Raid10Pattern* other = (const Raid10Pattern*) second;

   return defaultNumTargets == other->defaultNumTargets
      && stripeTargetIDs == other->stripeTargetIDs
      && mirrorTargetIDs == other->mirrorTargetIDs;
}
