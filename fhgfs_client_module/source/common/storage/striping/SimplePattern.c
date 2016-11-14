#include "SimplePattern.h"

bool SimplePattern_deserializePattern(StripePattern* this, DeserializeCtx* ctx)
{
   return true;
}

size_t SimplePattern_getStripeTargetIndex(StripePattern* this, int64_t pos)
{
   return 0;
}

uint16_t SimplePattern_getStripeTargetID(StripePattern* this, int64_t pos)
{
   return 0;
}

unsigned SimplePattern_getMinNumTargets(StripePattern* this)
{
   return 0;
}

unsigned SimplePattern_getDefaultNumTargets(StripePattern* this)
{
   return 0;
}

void SimplePattern_getStripeTargetIDsCopy(StripePattern* this, UInt16Vec* outTargetIDs)
{
   // nothing to be done here
}

UInt16Vec* SimplePattern_getStripeTargetIDs(StripePattern* this)
{
   return NULL;
}

