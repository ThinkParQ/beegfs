#include "BuildTypeTk.h"

/* Note: Do not add these functions to the header file, we want to explicitly check for values of
 *       the compiled library
 */

FhgfsBuildTypeDebug BuildTypeTk::getCommonLibDebugBuildType()
{
   return BUILDTYPE_CURRENT_DEBUG;
}
