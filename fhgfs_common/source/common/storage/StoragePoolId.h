#ifndef COMMON_STORAGEPOOLID_H
#define COMMON_STORAGEPOOLID_H

#include <common/NumericID.h>

/*
 * represents a numeric storage pool ID
 *
 * this inherits from NumericID and is not a simple typedef to achieve "type" safety with nodeIDs,
 * groupIDs, targetIDs, etc. (which might in fact all be based on the same base integer type, but
 * should be distinguishable)
 *
 * // Note: this must always be in sync with client's StoragePoolId!
 *
 */
class StoragePoolId: public NumericID<uint16_t>
{
   public:
      StoragePoolId(): NumericID<ValueT>() { }
      explicit StoragePoolId(ValueT value) : NumericID<ValueT>(value) { }
};

typedef std::list<StoragePoolId> StoragePoolIdList;
typedef StoragePoolIdList::iterator StoragePoolIdListIter;
typedef StoragePoolIdList::const_iterator StoragePoolIdListCIter;

typedef std::vector<StoragePoolId> StoragePoolIdVector;
typedef StoragePoolIdVector::iterator StoragePoolIdVectorIter;
typedef StoragePoolIdVector::const_iterator StoragePoolIdVectorCIter;

namespace std
{
   // specializing std::hash (e.g. to use in unordered maps)
   template<>
   struct hash<StoragePoolId>
   {
      public:
         size_t operator()(const StoragePoolId& obj) const
         {
            return std::hash<StoragePoolId::ValueT>()(obj.val());
         }
   };

   // specializing std::numeric_limits
   template<>
   class numeric_limits<StoragePoolId>
   {
      public:
         static StoragePoolId min()
         {
            return StoragePoolId(std::numeric_limits<StoragePoolId::ValueT>::min());
         };

         static StoragePoolId max()
         {
            return StoragePoolId(std::numeric_limits<StoragePoolId::ValueT>::max());
         };
   };
}

#endif /*COMMON_STORAGEPOOLID_H*/
