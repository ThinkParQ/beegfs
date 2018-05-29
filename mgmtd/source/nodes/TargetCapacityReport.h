#ifndef TARGETCAPACITYREPORT_H_
#define TARGETCAPACITYREPORT_H_

#include <map>
#include <stdint.h>

/**
 * Stores the part of a TargetStateInfo which is relevant for the InternodeSyncer to perform the
 * target capacity pools update. It is not necessary to have the targetID here since this is used
 * as a key in the TargetCapacityReportMap.
 */
struct TargetCapacityReport
{
   int64_t diskSpaceTotal;
   int64_t diskSpaceFree;
   int64_t inodesTotal;
   int64_t inodesFree;
};

typedef std::map<uint16_t, TargetCapacityReport> TargetCapacityReportMap; //targetID,capacityReport
typedef TargetCapacityReportMap::iterator TargetCapacityReportMapIter;
typedef TargetCapacityReportMap::const_iterator TargetCapacityReportMapCIter;

#endif /*TARGETCAPACITYREPORT_H_*/
