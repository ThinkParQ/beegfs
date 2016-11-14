#ifndef BUILDTYPETK_H_
#define BUILDTYPETK_H_

#include <common/Common.h>
#include <common/app/config/InvalidConfigException.h>


/**
 * This works by setting the build type of the common lib via getCommonLibBuildType() and setting
 * the build type of the program using the common lib vial getCurrentBuildType().
 *
 * Note: Of course, there are a number of cases that won't be detected with this (e.g. rebuilds
 * of a program without a "make clean" etc.) and there is not guarantee that the check code will
 * even be reached when the build types differ.
 */

enum FhgfsBuildType
   {FhgfsBuildType_RELEASE=0, FhgfsBuildType_DEBUG=1};


#ifdef FHGFS_DEBUG
#define BUILDTYPE_CURRENT FhgfsBuildType_DEBUG
#else
#define BUILDTYPE_CURRENT FhgfsBuildType_RELEASE
#endif


class BuildTypeTk
{
   public:
      static FhgfsBuildType getCommonLibBuildType();


   private:
      BuildTypeTk() {}

   public:
      // inliners

      static inline FhgfsBuildType getCurrentBuildType()
      {
         return BUILDTYPE_CURRENT;
      }

      static inline void checkBuildTypes() throw(InvalidConfigException)
      {
         if(getCurrentBuildType() != getCommonLibBuildType() )
            throw InvalidConfigException("Build types differ. Check your release/debug make "
               "settings.");
      }

};


#endif /* BUILDTYPETK_H_ */
