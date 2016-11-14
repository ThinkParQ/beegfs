#ifndef VERSIONTK_H_
#define VERSIONTK_H_


/*
 * Note: Remember to always keep version encoding in in sync with fhgfs_common's VersionTk.h
 */

/*
 * Size of BEEGFS_VERSION_CODE is 4 bytes (unsigned int). Format from highest to lowest byte:
 * 1 byte release_type number
 * 1 byte major version
 * 1 byte minor version
 * 1 byte release_num (always 0 for stable, can be set for beta or git builds)
 */

static inline bool VersionTk_checkRequiredRelease(unsigned requiredVersionCode,
   unsigned nodeVersionCode);


/**
 * Short-hand for BEEGFS_VERSION_NUM_STRIP(BEEGFS_VERSION_CODE).
 */
#define BEEGFS_VERSION_NUM \
   BEEGFS_VERSION_NUM_STRIP(BEEGFS_VERSION_CODE)

/**
 * Strip build type from BEEGFS_VERSION_CODE, so that the result can be compared to other version
 * numbers, e.g. by using BEEGFS_VERSION_NUM_ENCODE().
 */
#define BEEGFS_VERSION_NUM_STRIP(fhgfsVersionCode) \
   (0xFFFFFF & fhgfsVersionCode) /* 1st byte is release type, so keep only last 3 bytes */

/**
 * Decode individual version elements from version code.
 */
#define BEEGFS_VERSION_NUM_DECODE(fhgfsVersionCode, outMajor, outMinor, outReleaseNum) \
   do                                                                                  \
   {                                                                                   \
      outMajor      = (fhgfsVersionCode >> 16) & 0xFF);                                \
      outMinor      = (fhgfsVersionCode >>  8) & 0xFF;                                 \
      outReleaseNum = (fhgfsVersionCode      ) & 0xFF;                                 \
   } while(0)

/**
 * Encode fhgfs version code from major/minor/releaseNum and leave release type number out.
 *
 * @return unsigned int
 */
#define BEEGFS_VERSION_NUM_ENCODE(major, minor, releaseNum) \
   ( ( major << 16) + (minor << 8) + (releaseNum) )

/**
 * Short-hand for BEEGFS_VERSION_TYPE_STRIP(BEEGFS_VERSION_CODE).
 */
#define BEEGFS_VERSION_TYPE \
   BEEGFS_VERSION_TYPE_STRIP(BEEGFS_VERSION_CODE)

/**
 * Strip version numbers from BEEGFS_VERSION_CODE, so that the result can be compared to other build
 * types, e.g. by using BEEGFS_VERSION_TYPE_STRIP().
 */
#define BEEGFS_VERSION_TYPE_STRIP(fhgfsVersionCode) \
   (0xFF000000 & fhgfsVersionCode) /* only 1st byte is release type, so discard the rest) */



/**
 * Compare version codes.
 * Intended to more conveniently check whether a certain feature is supported by a given node.
 *
 * @param requiredVersionCode minimum required version for a certain feature.
 * @param nodeVersionCode the version code to check (usually from the node that we want to
 * test for a certain feature).
 * @return true if the node version code is equal or greater than required version code.
 */
bool VersionTk_checkRequiredRelease(unsigned requiredVersionCode, unsigned nodeVersionCode)
{
   unsigned requiredVersionCodeStrip = BEEGFS_VERSION_NUM_STRIP(requiredVersionCode);
   unsigned nodeVersionCodeStrip = BEEGFS_VERSION_NUM_STRIP(nodeVersionCode);

   return (nodeVersionCodeStrip >= requiredVersionCodeStrip);
}

#endif /* VERSIONTK_H_ */
