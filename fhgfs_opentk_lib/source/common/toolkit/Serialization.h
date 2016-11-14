#ifndef SERIALIZATION_H_
#define SERIALIZATION_H_


#include <common/Common.h>


#include <arpa/inet.h>


// byte order transformation macros for 16-, 32-, 64-bit types

#if BYTE_ORDER == BIG_ENDIAN // BIG_ENDIAN

   #define HOST_TO_LITTLE_ENDIAN_16(host, le)    le = byteswap16(host)
   #define HOST_TO_LITTLE_ENDIAN_32(host, le)    le = byteswap32(host)
   #define HOST_TO_LITTLE_ENDIAN_64(host, le)    le = byteswap64(host)

#else // LITTLE_ENDIAN

   #define HOST_TO_LITTLE_ENDIAN_16(host, le)   le = (host)
   #define HOST_TO_LITTLE_ENDIAN_32(host, le)   le = (host)
   #define HOST_TO_LITTLE_ENDIAN_64(host, le)   le = (host)

#endif // BYTE_ORDER


#define LITTLE_ENDIAN_TO_HOST_16(le, host)    HOST_TO_LITTLE_ENDIAN_16(le, host)
#define LITTLE_ENDIAN_TO_HOST_32(le, host)    HOST_TO_LITTLE_ENDIAN_32(le, host)
#define LITTLE_ENDIAN_TO_HOST_64(le, host)    HOST_TO_LITTLE_ENDIAN_64(le, host)


class Serialization
{
   public:


   private:
      Serialization() {}


   public:
      // inliners

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

      static inline uint16_t  ntohsTrans(uint16_t value)
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
