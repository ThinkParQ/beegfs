#ifndef NUMNODEID_H
#define NUMNODEID_H

#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/SerializationTypes.h>
#include <common/toolkit/StringTk.h>

// Note: this must always be in sync with server's NumNodeId!

struct NumNodeID;
typedef struct NumNodeID NumNodeID;

struct NumNodeID
{
      uint32_t value;
};

static inline void NumNodeID_set(NumNodeID* this, uint32_t value)
{
   this->value = value;
}

static inline bool NumNodeID_compare(const NumNodeID* this, const NumNodeID* other)
{
   return (this->value == other->value);
}

static inline bool NumNodeID_isZero(const NumNodeID* this)
{
   return (this->value == 0);
}

static inline char* NumNodeID_str(const NumNodeID* this)
{
   return StringTk_uintToStr(this->value);
}

extern void NumNodeID_serialize(SerializeCtx* ctx, const NumNodeID* this);
extern bool NumNodeID_deserialize(DeserializeCtx* ctx, NumNodeID* outThis);

#endif /* NUMNODEID_H */
