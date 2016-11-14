#ifndef SERIALIZATION_H_
#define SERIALIZATION_H_

#include <Common.h>

#include "Byteswap.h"


#define SERIALIZATION_NICLISTELEM_NAME_SIZE  (16)
#define SERIALIZATION_NICLISTELEM_SIZE       (5+SERIALIZATION_NICLISTELEM_NAME_SIZE) /*
                                              5 = 4b ipAddr + 1b nicType */
#define SERIALIZATION_CHUNKINFOLISTELEM_ID_SIZE (96)
#define SERIALIZATION_CHUNKINFOLISTELEM_PATHSTR_SIZE (255)
#define SERIALIZATION_FILEINFOLISTELEM_OWNERNODE_SIZE (255)


struct PathDeserializationInfo
{
   unsigned elemsBufLen;
   unsigned elemNum;
   const char* elemListStart;
   bool isAbsolute;
};

// forward declaration
class Path;


class Serialization
{
   public:
      // string (de)serialization
      static unsigned serializeStr(char* buf, unsigned strLen, const char* strStart);
      static bool deserializeStr(const char* buf, size_t bufLen,
         unsigned* outStrLen, const char** outStrStart, unsigned* outBufLen);
      static unsigned serialLenStr(unsigned strLen);

      // string aligned (de)serialization
      static unsigned serializeStrAlign4(char* buf, unsigned strLen, const char* strStart);
      static bool deserializeStrAlign4(const char* buf, size_t bufLen,
         unsigned* outStrLen, const char** outStrStart, unsigned* outBufLen);
      static unsigned serialLenStrAlign4(unsigned strLen);

      // stringList (de)serialization
      static unsigned serializeStringList(char* buf, const StringList* list);
      static bool deserializeStringListPreprocess(const char* buf, size_t bufLen,
         unsigned* outElemNum, const char** outListStart, unsigned* outListBufLen);
      static bool deserializeStringList(unsigned listBufLen, unsigned elemNum,
         const char* listStart, StringList* outList);
      static unsigned serialLenStringList(const StringList* list);

      // StringVector (de)serialization
      static unsigned serializeStringVec(char* buf, StringVector* vec);
      static bool deserializeStringVecPreprocess(const char* buf, size_t bufLen,
         unsigned* outElemNum, const char** outListStart, unsigned* outVecBufLen);
      static bool deserializeStringVec(unsigned listBufLen, unsigned elemNum,
         const char* listStart, StringVector* outVec);
      static unsigned serialLenStringVec(StringVector* vec);

      // IntVector (de)serialization
      static unsigned serializeIntVector(char* buf, IntVector* vec);
      static bool deserializeIntVectorPreprocess(const char* buf, size_t bufLen,
         unsigned* outElemNum, const char** outVecStart, unsigned* outVecBufLen);
      static bool deserializeIntVector(unsigned vecBufLen, unsigned elemNum,
         const char* outVecStart, IntVector* outVec);
      static unsigned serialLenIntVector(IntVector* vec);

      // Int64Vector (de)serialization
      static unsigned serializeInt64Vector(char* buf, Int64Vector* vec);
      static bool deserializeInt64VectorPreprocess(const char* buf, size_t bufLen,
         unsigned* outElemNum, const char** outVecStart, unsigned* outVecBufLen);
      static bool deserializeInt64Vector(unsigned vecBufLen, unsigned elemNum,
         const char* outVecStart, Int64Vector* outVec);
      static unsigned serialLenInt64Vector(Int64Vector* vec);

      // UInt64Vector (de)serialization
      static unsigned serializeUInt64Vector(char* buf, UInt64Vector* vec);
      static bool deserializeUInt64VectorPreprocess(const char* buf, size_t bufLen,
         unsigned* outElemNum, const char** outListStart, unsigned* outVecBufLen);
      static bool deserializeUInt64Vector(unsigned vecBufLen, unsigned elemNum,
         const char* outVecStart, UInt64Vector* outVec);
      static unsigned serialLenUInt64Vector(UInt64Vector* vec);
      static unsigned serialLenUInt64Vector(size_t size);

      // IntList (de)serialization
      static unsigned serializeIntList(char* buf, IntList* list);
      static bool deserializeIntListPreprocess(const char* buf, size_t bufLen,
         unsigned* outElemNum, const char** outListStart, unsigned* outListBufLen);
      static bool deserializeIntList(unsigned listBufLen, unsigned elemNum,
         const char* listStart, IntList* outList);
      static unsigned serialLenIntList(IntList* list);

      // Int64List (de)serialization
      static unsigned serializeInt64List(char* buf, Int64List* list);
      static bool deserializeInt64ListPreprocess(const char* buf, size_t bufLen,
         unsigned* outElemNum, const char** outListStart, unsigned* outListBufLen);
      static bool deserializeInt64List(unsigned listBufLen, unsigned elemNum,
         const char* listStart, Int64List* outList);
      static unsigned serialLenInt64List(Int64List* list);
      static unsigned serialLenInt64List(size_t size);

      // UInt64List (de)serialization
      static unsigned serializeUInt64List(char* buf, UInt64List* list);
      static bool deserializeUInt64ListPreprocess(const char* buf, size_t bufLen,
         unsigned* outElemNum, const char** outListStart, unsigned* outListBufLen);
      static bool deserializeUInt64List(unsigned listBufLen, unsigned elemNum,
         const char* listStart, UInt64List* outList);
      static unsigned serialLenUInt64List(UInt64List* list);
      static unsigned serialLenUInt64List(size_t size);

      // path (de)serialization
      static unsigned serializePath(char* buf, Path* path);
      static bool deserializePathPreprocess(const char* buf, size_t bufLen,
         struct PathDeserializationInfo* outInfo, unsigned* outBufLen);
      static bool deserializePath(struct PathDeserializationInfo& info, Path* outPath);
      static unsigned serialLenPath(Path* path);


   private:
      Serialization() {}


   public:
      // inliners

      // char (de)serialization
      static unsigned serializeChar(char* buf, char value)
      {
         *buf = value;

         return serialLenChar();
      }

      static bool deserializeChar(const char* buf, size_t bufLen,
         char* outValue, unsigned* outLen)
      {
         if(unlikely(bufLen < serialLenChar() ) )
            return false;

         *outLen = serialLenChar();
         *outValue = *buf;

         return true;
      }

      static unsigned serialLenChar()
      {
         return sizeof(char);
      }

      // bool (de)serialization
      static unsigned serializeBool(char* buf, bool value)
      {
         return serializeChar(buf, value ? 1 : 0);
      }

      static bool deserializeBool(const char* buf, size_t bufLen,
         bool* outValue, unsigned* outLen)
      {
         char charVal = 0;

         bool deserRes = deserializeChar(buf, bufLen, &charVal, outLen);

         *outValue = (charVal != 0);

         return deserRes;
      }

      static unsigned serialLenBool()
      {
         return serialLenChar();
      }

      // ushort (de)serialization
      static inline unsigned serializeUShort(char* buf, unsigned short value)
      {
         HOST_TO_LITTLE_ENDIAN_16(value, *(unsigned short*)buf);

         return serialLenUShort();
      }

      static inline bool deserializeUShort(const char* buf, size_t bufLen,
         unsigned short* outValue, unsigned* outLen)
      {
         if(unlikely(bufLen < serialLenUShort() ) )
            return false;

         *outLen = serialLenUShort();
         LITTLE_ENDIAN_TO_HOST_16(*(const unsigned short*)buf, *outValue);

         return true;
      }

      static inline unsigned serialLenUShort()
      {
         return sizeof(unsigned short);
      }
      
      // int (de)serialization
      static inline unsigned serializeInt(char* buf, int value)
      {
         return serializeUInt(buf, value);
      }

      static inline bool deserializeInt(const char* buf, size_t bufLen,
         int* outValue, unsigned* outLen)
      {
         return deserializeUInt(buf, bufLen, (unsigned*)outValue, outLen);
      }

      static inline unsigned serialLenInt()
      {
         return serialLenUInt();
      }

      // uint (de)serialization
      static inline unsigned serializeUInt(char* buf, unsigned value)
      {
         HOST_TO_LITTLE_ENDIAN_32(value, *(unsigned*)buf);

         return serialLenUInt();
      }

      static inline bool deserializeUInt(const char* buf, size_t bufLen,
         unsigned* outValue, unsigned* outLen)
      {
         if(unlikely(bufLen < serialLenUInt() ) )
            return false;

         *outLen = serialLenUInt();
         LITTLE_ENDIAN_TO_HOST_32(*(const unsigned*)buf, *outValue);

         return true;
      }

      static inline unsigned serialLenUInt()
      {
         return sizeof(unsigned);
      }

      // int64 (de)serialization
      static inline unsigned serializeInt64(char* buf, int64_t value)
      {
         return serializeUInt64(buf, value);
      }

      static inline bool deserializeInt64(const char* buf, size_t bufLen,
         int64_t* outValue, unsigned* outLen)
      {
         return deserializeUInt64(buf, bufLen, (uint64_t*)outValue, outLen);
      }

      static inline unsigned serialLenInt64()
      {
         return serialLenUInt64();
      }

      // uint64 (de)serialization
      static inline unsigned serializeUInt64(char* buf, uint64_t value)
      {
         HOST_TO_LITTLE_ENDIAN_64(value, *(uint64_t*)buf);

         return serialLenUInt64();
      }

      static inline bool deserializeUInt64(const char* buf, size_t bufLen,
         uint64_t* outValue, unsigned* outLen)
      {
         if(unlikely(bufLen < serialLenUInt64() ) )
            return false;

         *outLen = serialLenUInt64();
         LITTLE_ENDIAN_TO_HOST_64(*(const uint64_t*)buf, *outValue);

         return true;
      }

      static inline unsigned serialLenUInt64()
      {
         return sizeof(uint64_t);
      }

      static inline uint32_t htonlTrans(uint32_t value)
      {
         return htonl(value);
      }

      static inline uint32_t ntohlTrans(uint32_t value)
      {
         return ntohl(value);
      }

      static inline uint16_t htonsTrans(uint16_t value)
      {
         return htons(value);
      }

      static inline uint16_t ntohsTrans(uint16_t value)
      {
         return ntohs(value);
      }

      // host to network byte order (big endian) transformation for 64-bit types

      #if BYTE_ORDER == BIG_ENDIAN // BIG_ENDIAN

         static inline uint64_t htonllTrans(int64_t value)
         {
            return value;
         }

      #else // LITTLE_ENDIAN

         static inline uint64_t htonllTrans(int64_t value)
         {
            return byteswap64(value);
         }

      #endif


      static inline uint64_t ntohllTrans(int64_t value)
      {
         return htonllTrans(value);
      }

      static inline unsigned short byteswap16(unsigned short x)
      {
         char* xChars = (char*)&x;

         unsigned short retVal;
         char* retValChars = (char*)&retVal;

         retValChars[0] = xChars[1];
         retValChars[1] = xChars[0];

         return retVal;
      }

      static inline unsigned byteswap32(unsigned x)
      {
         char* xChars = (char*)&x;

         unsigned retVal;
         char* retValChars = (char*)&retVal;

         retValChars[0] = xChars[3];
         retValChars[1] = xChars[2];
         retValChars[2] = xChars[1];
         retValChars[3] = xChars[0];

         return retVal;
      }

      static inline uint64_t byteswap64(uint64_t x)
      {
         char* xChars = (char*)&x;

         uint64_t retVal;
         char* retValChars = (char*)&retVal;

         retValChars[0] = xChars[7];
         retValChars[1] = xChars[6];
         retValChars[2] = xChars[5];
         retValChars[3] = xChars[4];
         retValChars[4] = xChars[3];
         retValChars[5] = xChars[2];
         retValChars[6] = xChars[1];
         retValChars[7] = xChars[0];

         return retVal;
      }

};

#endif /*SERIALIZATION_H_*/
