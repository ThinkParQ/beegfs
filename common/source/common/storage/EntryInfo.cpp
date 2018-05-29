/*
 * EntryInfo methods
 */

#include <common/app/log/LogContext.h>
#include <common/storage/EntryInfo.h>
#include <common/toolkit/serialization/Serialization.h>

#define MIN_ENTRY_ID_LEN 1 // usually A-B-C, but also "root" and "disposal"

bool EntryInfo::operator==(const EntryInfo& other) const
{
   return ownerNodeID == other.ownerNodeID
      && parentEntryID == other.parentEntryID
      && entryID == other.entryID
      && fileName == other.fileName
      && entryType == other.entryType
      && featureFlags == other.featureFlags;
}
