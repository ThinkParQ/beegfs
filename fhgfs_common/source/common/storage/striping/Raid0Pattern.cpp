#include "Raid0Pattern.h"

bool Raid0Pattern::updateStripeTargetIDs(StripePattern* updatedStripePattern)
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

bool Raid0Pattern::patternEquals(const StripePattern* second) const
{
   const Raid0Pattern* other = (const Raid0Pattern*) second;

   return defaultNumTargets == other->defaultNumTargets
      && stripeTargetIDs == other->stripeTargetIDs;
}
