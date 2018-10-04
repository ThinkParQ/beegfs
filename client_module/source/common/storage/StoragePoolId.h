#ifndef CLIENT_STORAGEPOOLID_H
#define CLIENT_STORAGEPOOLID_H

#include <common/toolkit/SerializationTypes.h>
#include <common/toolkit/StringTk.h>

// keep in sync with values from server's StoragePoolStore
#define STORAGEPOOLID_INVALIDPOOLID 0

// Note: this must always be in sync with server's StoragePoolId!
struct StoragePoolId;
typedef struct StoragePoolId StoragePoolId;

struct StoragePoolId
{
      uint16_t value;
};

static inline void StoragePoolId_set(StoragePoolId* this, uint16_t value)
{
   this->value = value;
}

static inline bool StoragePoolId_compare(const StoragePoolId* this, const StoragePoolId* other)
{
   return (this->value == other->value);
}

static inline char* StoragePoolId_str(const StoragePoolId* this)
{
   return StringTk_uintToStr(this->value);
}

extern void StoragePoolId_serialize(SerializeCtx* ctx, const StoragePoolId* this);
extern bool StoragePoolId_deserialize(DeserializeCtx* ctx, StoragePoolId* outThis);

#endif /* CLIENT_STORAGEPOOLID_H */
