#ifndef QUOTACONFIG_H_
#define QUOTACONFIG_H_


#include <common/storage/quota/QuotaData.h>
#include <common/storage/StoragePoolId.h>


/**
 * A parent struct for all struct with quota configurations
 */
struct QuotaConfig
{
   QuotaDataType cfgType;        // the type of the IDs: QuotaDataType_...
   unsigned cfgID;               // a single UID/GID
   bool cfgUseAll;               // true if all available UIDs/GIDs needed
   bool cfgUseList;              // true if a UID/GID list is given
   bool cfgUseRange;             // true if a UID/GID range is given
   bool cfgCsv;                  // true if csv print (no units and csv) needed
   unsigned cfgIDRangeStart;     // the first UIDs/GIDs of a range to collect the quota data/limits
   unsigned cfgIDRangeEnd;       // the last UIDs/GIDs of a range to collect the quota data/limits
   UIntList cfgIDList;           // the list of UIDs/GIDs to collect the quota data/limits
   bool cfgDefaultLimits;        // true if default quota limits should be set/requested
   StoragePoolId cfgStoragePoolId;
};


#endif /* QUOTACONFIG_H_ */
