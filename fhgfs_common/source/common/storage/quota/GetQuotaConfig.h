#ifndef GETQUOTACONFIG_H_
#define GETQUOTACONFIG_H_

#include "QuotaConfig.h"


#define GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST                0
#define GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET     1
#define GETQUOTACONFIG_SINGLE_TARGET                          2


/**
 * A struct for all configurations which are required to collect quota data/limits
 */
struct GetQuotaInfoConfig : public QuotaConfig
{
   unsigned cfgTargetSelection;  // the kind to collect the quota data/limits: GETQUOTACONFIG_...
   uint16_t cfgTargetNumID;      // targetNumID if a single target is selected
   bool cfgPrintUnused;          // print users/groups that have no space used
   bool cfgWithSystemUsersGroups;// if system users/groups should be used be considered
};


#endif /* GETQUOTACONFIG_H_ */
