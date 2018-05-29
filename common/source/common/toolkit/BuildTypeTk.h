#ifndef BUILDTYPETK_H_
#define BUILDTYPETK_H_

#include <common/Common.h>
#include <common/app/config/InvalidConfigException.h>


/**
 * This works by setting the build type of the common lib via getCommonLibDebugBuildType() and setting
 * the build type of the program using the common lib vial getCurrentDebugBuildType().
 *
 * Note: Of course, there are a number of cases that won't be detected with this (e.g. rebuilds
 * of a program without a "make clean" etc.) and there is not guarantee that the check code will
 * even be reached when the build types differ.
 */

enum FhgfsBuildTypeDebug
   {FhgfsBuildType_DEBUG_OFF=0, FhgfsBuildType_DEBUG_ON=1};

#ifdef BEEGFS_DEBUG
#define BUILDTYPE_CURRENT_DEBUG FhgfsBuildType_DEBUG_ON
#else
#define BUILDTYPE_CURRENT_DEBUG FhgfsBuildType_DEBUG_OFF
#endif

class BuildTypeTk
{
   public:
      static FhgfsBuildTypeDebug getCommonLibDebugBuildType();

   private:
      BuildTypeTk() {}

   public:
      // inliners

      static inline FhgfsBuildTypeDebug getCurrentDebugBuildType()
      {
         return BUILDTYPE_CURRENT_DEBUG;
      }

      static inline void checkDebugBuildTypes()
      {
         if(getCurrentDebugBuildType() != getCommonLibDebugBuildType() )
            throw InvalidConfigException("Debug build types differ. Check your release/debug make "
               "settings.");
      }
};


#endif /* BUILDTYPETK_H_ */
