#ifndef SERIALIZATION_H_
#define SERIALIZATION_H_

#include <common/Common.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/NicAddressList.h>
#include <common/toolkit/list/Int64CpyList.h>
#include <common/toolkit/list/Int64CpyListIter.h>
#include <common/toolkit/list/NumNodeIDList.h>
#include <common/toolkit/list/NumNodeIDListIter.h>
#include <common/toolkit/list/StrCpyList.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/toolkit/list/UInt8List.h>
#include <common/toolkit/list/UInt8ListIter.h>
#include <common/toolkit/list/UInt16List.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/toolkit/vector/Int64CpyVec.h>
#include <common/toolkit/vector/StrCpyVec.h>
#include <common/toolkit/vector/UInt16Vec.h>
#include <common/toolkit/vector/UInt8Vec.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/Serialization.h>
#include <common/storage/Path.h>
#include <common/nodes/Node.h>
#include <common/nodes/NodeList.h>
#include <common/nodes/NodeListIter.h>
#include <os/OsCompat.h>

#include <asm/unaligned.h>


#define SERIALIZATION_NICLISTELEM_NAME_SIZE  (16)
#define SERIALIZATION_NICLISTELEM_SIZE       (8+SERIALIZATION_NICLISTELEM_NAME_SIZE) /*
                                              8 = 4b ipAddr + 1b nicType + 3b alignment padding */


bool __Serialization_deserializeNestedField(DeserializeCtx* ctx, DeserializeCtx* outCtx);

// string (de)serialization
void Serialization_serializeStr(SerializeCtx* ctx, unsigned strLen, const char* strStart);
bool Serialization_deserializeStr(DeserializeCtx* ctx,
   unsigned* outStrLen, const char** outStrStart);

// string aligned (de)serialization
void Serialization_serializeStrAlign4(SerializeCtx* ctx, unsigned strLen, const char* strStart);
bool Serialization_deserializeStrAlign4(DeserializeCtx* ctx,
   unsigned* outStrLen, const char** outStrStart);

// char array (arbitrary binary data) (de)serialization
void Serialization_serializeCharArray(SerializeCtx* ctx, unsigned arrLen, const char* arrStart);
bool Serialization_deserializeCharArray(DeserializeCtx* ctx,
   unsigned* outArrLen, const char** outArrStart, unsigned* outLen);

// nicList (de)serialization
void Serialization_serializeNicList(SerializeCtx* ctx, NicAddressList* nicList);
bool Serialization_deserializeNicListPreprocess(DeserializeCtx* ctx, RawList* outList);
void Serialization_deserializeNicList(const RawList* inList, NicAddressList* outNicList);

// nodeList (de)serialization
bool Serialization_deserializeNodeListPreprocess(DeserializeCtx* ctx, RawList* outList);
void Serialization_deserializeNodeList(App* app, const RawList* inList, NodeList* outNodeList);

// strCpyList (de)serialization
void Serialization_serializeStrCpyList(SerializeCtx* ctx, StrCpyList* list);
bool Serialization_deserializeStrCpyListPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeStrCpyList(const RawList* inList, StrCpyList* outList);

// strCpyVec (de)serialization
void Serialization_serializeStrCpyVec(SerializeCtx* ctx, StrCpyVec* vec);
bool Serialization_deserializeStrCpyVecPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeStrCpyVec(const RawList* inList, StrCpyVec* outVec);

// uint8List (de)serialization
bool Serialization_deserializeUInt8ListPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeUInt8List(const RawList* inList, UInt8List* outList);

// uint8Vec (de)serialization
bool Serialization_deserializeUInt8VecPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeUInt8Vec(const RawList* inList, UInt8Vec* outList);

// uint16List (de)serialization
void Serialization_serializeUInt16List(SerializeCtx* ctx, UInt16List* list);
bool Serialization_deserializeUInt16ListPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeUInt16List(const RawList* inList, UInt16List* outList);

// uint16Vec (de)serialization
void Serialization_serializeUInt16Vec(SerializeCtx* ctx, UInt16Vec* vec);
bool Serialization_deserializeUInt16VecPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeUInt16Vec(const RawList* inList, UInt16Vec* outVec);

// int64CpyList (de)serialization
void Serialization_serializeInt64CpyList(SerializeCtx* ctx, Int64CpyList* list);
bool Serialization_deserializeInt64CpyListPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeInt64CpyList(const RawList* inList, Int64CpyList* outList);

// uint64Vec (de)serialization
void Serialization_serializeInt64CpyVec(SerializeCtx* ctx, Int64CpyVec* vec);
bool Serialization_deserializeInt64CpyVecPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeInt64CpyVec(const RawList* inList, Int64CpyVec* outVec);

// NumNodeIDList (de)serialization
bool Serialization_deserializeNumNodeIDListPreprocess(DeserializeCtx* ctx, RawList* outList);
bool Serialization_deserializeNumNodeIDList(const RawList* inList, NumNodeIDList* outList);


// inliners
static inline void Serialization_serializeBlock(SerializeCtx* ctx, const void* value,
   unsigned length);

static inline unsigned Serialization_serialLenChar(void);
static inline void Serialization_serializeChar(SerializeCtx* ctx, char value);
static inline bool Serialization_deserializeChar(DeserializeCtx* ctx, char* outValue);
static inline unsigned Serialization_serialLenBool(void);
static inline void Serialization_serializeBool(SerializeCtx* ctx, bool value);
static inline bool Serialization_deserializeBool(DeserializeCtx* ctx, bool* outValue);
static inline unsigned Serialization_serialLenUInt8(void);
static inline bool Serialization_deserializeUInt8(DeserializeCtx* ctx, uint8_t* outValue);
static inline unsigned Serialization_serialLenShort(void);
static inline void Serialization_serializeShort(SerializeCtx* ctx, short value);
static inline bool Serialization_deserializeShort(DeserializeCtx* ctx, short* outValue);
static inline unsigned Serialization_serialLenUShort(void);
static inline void Serialization_serializeUShort(SerializeCtx* ctx, unsigned short value);
static inline bool Serialization_deserializeUShort(DeserializeCtx* ctx, unsigned short* outValue);
static inline void Serialization_serializeInt(SerializeCtx* ctx, int value);
static inline bool Serialization_deserializeInt(DeserializeCtx* ctx, int* outValue);
static inline unsigned Serialization_serialLenInt(void);
static inline void Serialization_serializeUInt(SerializeCtx* ctx, unsigned value);
static inline bool Serialization_deserializeUInt(DeserializeCtx* ctx, unsigned* outValue);
static inline unsigned Serialization_serialLenUInt(void);
static inline void Serialization_serializeInt64(SerializeCtx* ctx, int64_t value);
static inline bool Serialization_deserializeInt64(DeserializeCtx* ctx, int64_t* outValue);
static inline unsigned Serialization_serialLenInt64(void);
static inline void Serialization_serializeUInt64(SerializeCtx* ctx, uint64_t value);
static inline bool Serialization_deserializeUInt64(DeserializeCtx* ctx, uint64_t* outValue);
static inline unsigned Serialization_serialLenUInt64(void);


void Serialization_serializeBlock(SerializeCtx* ctx, const void* value, unsigned length)
{
   if(ctx->data)
      memcpy(ctx->data + ctx->length, value, length);

   ctx->length += length;
}

#define __Serialization_deserializeRaw(ctx, convert, outValue) \
   ({ \
      DeserializeCtx* ctx__ = (ctx); \
      bool result = false; \
      (void) (outValue == (__typeof__(convert(ctx__->data) )*) 0); \
      if(likely(ctx__->length >= sizeof(*(outValue) ) ) ) \
      { \
         *(outValue) = convert(ctx__->data); \
         ctx__->data += sizeof(*(outValue) ); \
         ctx__->length -= sizeof(*(outValue) ); \
         result = true; \
      } \
      result; \
   })


// char (de)serialization
static inline unsigned Serialization_serialLenChar(void)
{
   return sizeof(char);
}

static inline void Serialization_serializeChar(SerializeCtx* ctx, char value)
{
   if(ctx->data)
      ctx->data[ctx->length] = value;

   ctx->length += sizeof(value);
}

static inline bool Serialization_deserializeChar(DeserializeCtx* ctx, char* outValue)
{
   return __Serialization_deserializeRaw(ctx, *, outValue);
}


// bool (de)serialization
static inline unsigned Serialization_serialLenBool(void)
{
   return Serialization_serialLenChar();
}

static inline void Serialization_serializeBool(SerializeCtx* ctx, bool value)
{
   Serialization_serializeChar(ctx, value ? 1 : 0);
}

static inline bool Serialization_deserializeBool(DeserializeCtx* ctx, bool* outValue)
{
   char charVal = 0;
   bool deserRes = Serialization_deserializeChar(ctx, &charVal);

   *outValue = (charVal != 0);
   return deserRes;
}

// uint8_t (de)serialization
static inline unsigned Serialization_serialLenUInt8(void)
{
   return Serialization_serialLenChar();
}

static inline bool Serialization_deserializeUInt8(DeserializeCtx* ctx,
   uint8_t* outValue)
{
   return Serialization_deserializeChar(ctx, (char*)outValue);
}

// short (de)serialization
static inline unsigned Serialization_serialLenShort(void)
{
   return Serialization_serialLenUShort();
}

static inline void Serialization_serializeShort(SerializeCtx* ctx, short value)
{
   Serialization_serializeUShort(ctx, value);
}

static inline bool Serialization_deserializeShort(DeserializeCtx* ctx,
   short* outValue)
{
   return Serialization_deserializeUShort(ctx, (unsigned short*)outValue);
}

// ushort (de)serialization
static inline unsigned Serialization_serialLenUShort(void)
{
   return sizeof(unsigned short);
}

static inline void Serialization_serializeUShort(SerializeCtx* ctx, unsigned short value)
{
   if(ctx->data)
      put_unaligned_le16(value, ctx->data + ctx->length);

   ctx->length += sizeof(value);
}

static inline bool Serialization_deserializeUShort(DeserializeCtx* ctx, unsigned short* outValue)
{
   return __Serialization_deserializeRaw(ctx, get_unaligned_le16, outValue);
}

// int (de)serialization
static inline unsigned Serialization_serialLenInt(void)
{
   return Serialization_serialLenUInt();
}

static inline void Serialization_serializeInt(SerializeCtx* ctx, int value)
{
   Serialization_serializeUInt(ctx, value);
}

static inline bool Serialization_deserializeInt(DeserializeCtx* ctx, int* outValue)
{
   return Serialization_deserializeUInt(ctx, (unsigned*)outValue);
}


// uint (de)serialization
static inline void Serialization_serializeUInt(SerializeCtx* ctx, unsigned value)
{
   if(ctx->data)
      put_unaligned_le32(value, ctx->data + ctx->length);

   ctx->length += sizeof(value);
}

static inline bool Serialization_deserializeUInt(DeserializeCtx* ctx, unsigned* outValue)
{
   return __Serialization_deserializeRaw(ctx, get_unaligned_le32, outValue);
}

static inline unsigned Serialization_serialLenUInt(void)
{
   return sizeof(unsigned);
}

// int64 (de)serialization
static inline void Serialization_serializeInt64(SerializeCtx* ctx, int64_t value)
{
   Serialization_serializeUInt64(ctx, value);
}

static inline bool Serialization_deserializeInt64(DeserializeCtx* ctx,
   int64_t* outValue)
{
   return Serialization_deserializeUInt64(ctx, (uint64_t*)outValue);
}

static inline unsigned Serialization_serialLenInt64(void)
{
   return Serialization_serialLenUInt64();
}

// uint64 (de)serialization
static inline void Serialization_serializeUInt64(SerializeCtx* ctx, uint64_t value)
{
   if(ctx->data)
      put_unaligned_le64(value, ctx->data + ctx->length);

   ctx->length += sizeof(value);
}

static inline bool Serialization_deserializeUInt64(DeserializeCtx* ctx,
   uint64_t* outValue)
{
   return __Serialization_deserializeRaw(ctx, get_unaligned_le64, outValue);
}

static inline unsigned Serialization_serialLenUInt64(void)
{
   return sizeof(uint64_t);
}

#endif /*SERIALIZATION_H_*/
