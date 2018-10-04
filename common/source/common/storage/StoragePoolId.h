#ifndef COMMON_STORAGEPOOLID_H
#define COMMON_STORAGEPOOLID_H

#include <common/NumericID.h>

/*
 * represents a numeric storage pool ID
 *
 * // Note: this must always be in sync with client's StoragePoolId!
 *
 */
enum StoragePoolIdTag {};
typedef NumericID<uint16_t, StoragePoolIdTag> StoragePoolId;

typedef std::list<StoragePoolId> StoragePoolIdList;
typedef StoragePoolIdList::iterator StoragePoolIdListIter;
typedef StoragePoolIdList::const_iterator StoragePoolIdListCIter;

typedef std::vector<StoragePoolId> StoragePoolIdVector;
typedef StoragePoolIdVector::iterator StoragePoolIdVectorIter;
typedef StoragePoolIdVector::const_iterator StoragePoolIdVectorCIter;

#endif /*COMMON_STORAGEPOOLID_H*/
