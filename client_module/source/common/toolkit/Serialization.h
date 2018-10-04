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
static inline void Serialization_serializeUInt8(SerializeCtx* ctx, uint8_t value)
{
   Serialization_serializeChar(ctx, (char) value);
}

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

#define __SERDES_CONCAT_I(a, b) a ## b
#define __SERDES_CONCAT(a, b) __SERDES_CONCAT_I(a, b)

#define __SERDES_COUNT_I(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, count, ...) count
#define __SERDES_COUNT(...) __SERDES_COUNT_I(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define __SERDES_CALL(Base, ...) __SERDES_CONCAT(Base, __SERDES_COUNT(__VA_ARGS__))(__VA_ARGS__)

#define __SERDES_FIELD_NAME(Name, SerMod, NS, Suffix) Name
#define __SERDES_FIELD_SER(Name, SerMod, NS, Suffix) NS##_serialize##Suffix
#define __SERDES_FIELD_SER_MOD(Name, SerMod, NS, Suffix) SerMod
#define __SERDES_FIELD_DES(Name, SerMod, NS, Suffix) NS##_deserialize##Suffix

#define __SERDES_DEFINE_SER_1(Field) \
   __SERDES_FIELD_SER Field(ctx, __SERDES_FIELD_SER_MOD Field(value->__SERDES_FIELD_NAME Field));
#define __SERDES_DEFINE_SER_2(Field, ...)  __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_1(__VA_ARGS__)
#define __SERDES_DEFINE_SER_3(Field, ...)  __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_2(__VA_ARGS__)
#define __SERDES_DEFINE_SER_4(Field, ...)  __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_3(__VA_ARGS__)
#define __SERDES_DEFINE_SER_5(Field, ...)  __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_4(__VA_ARGS__)
#define __SERDES_DEFINE_SER_6(Field, ...)  __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_5(__VA_ARGS__)
#define __SERDES_DEFINE_SER_7(Field, ...)  __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_6(__VA_ARGS__)
#define __SERDES_DEFINE_SER_8(Field, ...)  __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_7(__VA_ARGS__)
#define __SERDES_DEFINE_SER_9(Field, ...)  __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_8(__VA_ARGS__)
#define __SERDES_DEFINE_SER_10(Field, ...) __SERDES_DEFINE_SER_1(Field) __SERDES_DEFINE_SER_9(__VA_ARGS__)

#define __SERDES_DEFINE_SER(NS, Type, ...) \
   __attribute__((unused)) \
   void NS##_serialize(SerializeCtx* ctx, const Type* value) \
   { \
      __SERDES_CALL(__SERDES_DEFINE_SER_, __VA_ARGS__) \
   }


#define __SERDES_DEFINE_DES_1(Field) \
   if (!__SERDES_FIELD_DES Field(ctx, &value->__SERDES_FIELD_NAME Field)) \
      goto err;
#define __SERDES_DEFINE_DES_2(Field, ...)  __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_1(__VA_ARGS__)
#define __SERDES_DEFINE_DES_3(Field, ...)  __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_2(__VA_ARGS__)
#define __SERDES_DEFINE_DES_4(Field, ...)  __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_3(__VA_ARGS__)
#define __SERDES_DEFINE_DES_5(Field, ...)  __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_4(__VA_ARGS__)
#define __SERDES_DEFINE_DES_6(Field, ...)  __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_5(__VA_ARGS__)
#define __SERDES_DEFINE_DES_7(Field, ...)  __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_6(__VA_ARGS__)
#define __SERDES_DEFINE_DES_8(Field, ...)  __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_7(__VA_ARGS__)
#define __SERDES_DEFINE_DES_9(Field, ...)  __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_8(__VA_ARGS__)
#define __SERDES_DEFINE_DES_10(Field, ...) __SERDES_DEFINE_DES_1(Field) __SERDES_DEFINE_DES_9(__VA_ARGS__)

#define __SERDES_DEFINE_DES(NS, Type, Init, ErrFini, ...) \
   __attribute__((unused)) \
   bool NS##_deserialize(DeserializeCtx* ctx, Type* value) \
   { \
      Init; \
      __SERDES_CALL(__SERDES_DEFINE_DES_, __VA_ARGS__) \
      return true; \
   err: \
      ErrFini; \
      return false; \
   }


/**
 * SERDES_DECLARE_SERIALIZERS() - declares serialize and deserialize functions for a given type.
 * @NS namespace prefix for the generated functions
 * @Type the type to be made serializable/deserializable
 *
 * creates ``extern`` declarations for serializer functions as generated by
 * ``SERDES_DEFINE_SERIALIZERS`` or ``SERDES_DEFINE_SERIALIZERS_SIMPLE``.
 */
#define SERDES_DECLARE_SERIALIZERS(NS, Type) \
   extern void NS##_serialize(SerializeCtx* ctx, const Type* value); \
   extern bool NS##_deserialize(DeserializeCtx* ctx, Type* value);
/**
 * SERDES_DEFINE_SERIALIZERS() - defines serialize and deserialize functions for a given type.
 * @Access access modifiers for the generated functions
 * @NS namespace prefix for the generated functions
 * @Type the type to be made serializable/deserializable
 * @DesInit initialization code to be run before an instance is deserialized
 * @DesErrFini finalization code to be run when deserialization fails
 * @... description of @Type
 *
 * Declares a function ``NS##_serialize(SerializeCtx* ctx, const Type* value)`` and a function
 * ``NS##_deserialize(DeserializeCtx* ctx, Type* value)`` that follow the
 * serializer and deserializer conventions.
 *
 * During deserialization, ``DesInit`` is evaluated as an expression in the scope of the
 * deserializer function (ie, ``ctx`` and ``value`` are available). @DesInit may be called on
 * objects that have been initialized previously, via @DesInit or some other method. The invoker
 * must be careful to not introduce memory leaks via this expression. @DesInit and @DesErrFini
 * should always call the usual initialization and release functions for the type.
 *
 * If deserialization fails for any reason, ``DesErrFini`` is evaluated in the context of the
 * deserializer function just like ``DesInit``. All resources allocated during initialization or
 * deserialization must be released by ``DesErrFini``.
 *
 * @Type is exposed to the serializer as a list if one to 10 comma-separated tuples of the form
 * ``(FieldName, SerMod, SerNS, Suffix)``.
 *
 *  * ``FieldName`` is the name of the serialized field. Fields are serialized and deserialized in
 *    the order they appear in the description list.
 *  * ``SerMod`` is a modifier applied to the serialized value before passing it on to the
 *    serializer (see example).
 *  * ``SerNS`` is the namespace of the serializer and deserializer functions for this field.
 *  * ``Suffix`` is appended to the canonical name of the serializer and deserializer functions for
 *    the field.
 *
 * For the generated serializer, each field expands into a single call to a serializer.
 * ``SerNS##_serialize##Suffix(ctx, SerMod (value->FieldName);``
 *
 * For the generated deserializer, each field expands into a single call to a deserializer. For
 * deserializers the ``SerMod`` field of the description is ignored and a pointer is always passed.
 * ``SerNS##_deserialize##Suffix(ctx, &(value->FieldName));``
 *
 * Example:
 * ``
 *   struct S
 *   {
 *      const char* str;
 *   };
 *   SERDES_DEFINE_SERIALIZERS(static inline, _S, struct S,
 *      value->str = NULL,
 *      kfree(value->str),
 *      (str, &, Serdes, CStr))
 * ``
 * will expand to this serializer and deserializer code:
 * ``
 * static inline void _S_serialize(SerializeCtx* ctx, const struct S* value)
 * {
 *    Serdes_serializeCStr(ctx, &(value->str));
 * }
 * static inline bool _S_deserialize(DeserializeCtx* ctx, struct S* value)
 * {
 *    value->str = NULL;
 *    if (!Serdes_deserializeCStr(ctx, &value->str))
 *       goto err;
 *    return true;
 * err:
 *    kfree(value->str);
 *    return false;
 * }
 * ``
 */
#define SERDES_DEFINE_SERIALIZERS(Access, NS, Type, DesInit, DesErrFini, ...) \
   Access __SERDES_DEFINE_SER(NS, Type, __VA_ARGS__) \
   Access __SERDES_DEFINE_DES(NS, Type, DesInit, DesErrFini, __VA_ARGS__)
/**
 * SERDES_DEFINE_SERIALIZERS_SIMPLE() - defines serialize and deserialize functions for a given type
 * @Access access modifiers for the generated functions
 * @NS namespace prefix for the generated functions
 * @Type the type to be made serializable/deserializable
 * @... description of @Type
 *
 * expands to ``SERDES_DEFINE_SERIALIZERS(Access, NS, Type, ({}), ({}), __VA_ARGS__)``, ie no
 * special intialization or cleanup is done for deserialization.
 */
#define SERDES_DEFINE_SERIALIZERS_SIMPLE(Access, NS, Type, ...) \
   SERDES_DEFINE_SERIALIZERS(Access, NS, Type, ({}), ({}), __VA_ARGS__)

/**
 * SERDES_SERIALIZE_AS - defines enum serializers based on underlying type
 * @Access access modifiers for the generated functions
 * @NS namespace prefix for the generated functions
 * @Type the type to be made serializable/deserializable
 * @As underlying type to used for serialized data
 * @AsSuffix suffix for de/serializer in ``Serialization``
 *
 * enum types that go over the wire must be serialized with a consistent type at both end. this
 * macro defines serializers for a given enum @Type which use @As for the binary representation.
 * since this is about underlying types this macro can only call ``Serialization_*`` functions.
 */
#define SERDES_SERIALIZE_AS(Access, NS, Type, As, AsSuffix) \
   __attribute__((unused)) \
   Access void NS##_serialize(SerializeCtx* ctx, Type value) \
   { \
      Serialization_serialize##AsSuffix(ctx, (As) value); \
   } \
   __attribute__((unused)) \
   Access bool NS##_deserialize(DeserializeCtx* ctx, Type* value) \
   { \
      As tmp; \
      if (!Serialization_deserialize##AsSuffix(ctx, &tmp)) \
         return false; \
      *value = (Type) tmp; \
      return true; \
   }

/**
 * SERDES_DECLARE_LIST_SERIALIZERS() - declare list serializers for a given type
 * @NS namespace prefix for the generated functions
 * @Type type of list entry
 *
 * Declares ``extern`` prototypes of the functions as generated by
 * ``SERDES_DEFINE_LIST_SERIALIZERS``
 */
#define SERDES_DECLARE_LIST_SERIALIZERS(NS, Type) \
   extern void NS##_serialize(SerializeCtx* ctx, const struct list_head* list); \
   extern bool NS##_deserialize(DeserializeCtx* ctx, struct list_head* list); \
/**
 * SERDES_DEFINE_LIST_SERIALIZERS() - defines list serializers for a given type
 * @Access access modifiers for the generated functions
 * @NS namespace prefix for the generated functions
 * @Type type of list entry
 * @TypeNS namespace of serializer/deserializer functions for @Type
 * @TypeFini cleanup expression for a @Type
 * @ListMember name of the &struct list_head for this list in @Type
 * @HasLength %true if the binary representation of the list required a length field, %false if it
 *            must not have a length field
 *
 * Defines two functions ``void NS##_serialize(SerializeCtx* ctx, const struct list_head* list)``
 * and ``bool NS##_deserialize(DeserializeCtx* ctx, struct list_head* list)`` for serialization and
 * deserialization of lists.
 *
 * For serialization, each field is serialized in order by calling ``TypeNS##_serialize``.
 *
 * For deserialization, each field is deserialized into a newly kmalloc()ed element by calling
 * ``TypeNS##_deserialize``. Correctly deserialized elements are appended to the provided
 * &struct list_head. If deserialization fails the expression ``TypeFini(entry)`` is evaluated for
 * each successfully deserialized element, ``list`` is left unchanged.
 */
#define SERDES_DEFINE_LIST_SERIALIZERS(Access, NS, Type, TypeNS, TypeFini, ListMember, HasLength) \
   __attribute__((unused)) \
   Access void NS##_serialize(SerializeCtx* ctx, const struct list_head* list) \
   { \
      unsigned items = 0; \
      Type* item; \
      SerializeCtx ctxAtStart = *ctx; \
      if (HasLength) \
         Serialization_serializeUInt(ctx, 0); /* total field size */ \
      Serialization_serializeUInt(ctx, 0); /* number of elements */ \
      list_for_each_entry(item, list, ListMember) \
      { \
         TypeNS##_serialize(ctx, item); \
         items += 1; \
      } \
      if (HasLength) \
         Serialization_serializeUInt(&ctxAtStart, ctx->length - ctxAtStart.length); \
      Serialization_serializeUInt(&ctxAtStart, items); \
   } \
   __attribute__((unused)) \
   Access bool NS##_deserialize(DeserializeCtx* ctx, struct list_head* list) \
   { \
      DeserializeCtx innerCtx = {0, 0}; \
      DeserializeCtx* usedCtx = NULL; \
      unsigned items = 0; \
      LIST_HEAD(desItems); \
      \
      if (HasLength) { \
         if (!__Serialization_deserializeNestedField(ctx, &innerCtx)) \
            return false; \
         usedCtx = &innerCtx; \
      } else { \
         usedCtx = ctx; \
      } \
      if (!Serialization_deserializeUInt(usedCtx, &items)) \
         return false; \
      while (items-- > 0) \
      { \
         Type* entry = kmalloc(sizeof(*entry), GFP_NOFS); \
         if (!entry) \
            goto err; \
         if (!TypeNS##_deserialize(usedCtx, entry)) \
         { \
            kfree(entry); \
            goto err; \
         } \
         list_add_tail(&entry->ListMember, &desItems); \
      } \
      list_splice_tail(&desItems, list); \
      return true; \
   err: \
      BEEGFS_KFREE_LIST_DTOR(&desItems, Type, ListMember, TypeFini); \
      return false; \
   }


/**
 * SERDES_DEFINE_ENUM_SERIALIZERS() - defines static inline serializers for an enum type with a
 * given underlying type.
 * @NS namespace prefix for the generated functions
 * @EnumTy full enum type
 * @Underlying underlying integer type of the enum
 * @SerdesSuffix suffix for the ``Serialization_`` functions that serialize the underlying type
 */
#define SERDES_DEFINE_ENUM_SERIALIZERS(NS, EnumTy, Underlying, SerdesSuffix) \
   static inline void NS##_serialize(SerializeCtx* ctx, EnumTy value) \
   { \
      Serialization_serialize##SerdesSuffix(ctx, (Underlying) value); \
   } \
   static inline bool NS##_deserialize(DeserializeCtx* ctx, EnumTy* value) \
   { \
      Underlying raw; \
      if (!Serialization_deserialize##SerdesSuffix(ctx, &raw)) \
         return false; \
      *value = (EnumTy) raw; \
      return true; \
   }

#endif /*SERIALIZATION_H_*/
