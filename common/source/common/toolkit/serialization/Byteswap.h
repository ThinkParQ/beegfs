/*
 * Big- and little-endian byte swapping.
 */

#ifndef BYTESWAP_H_
#define BYTESWAP_H_

#include <inttypes.h>

// byte order transformation macros for 16-, 32-, 64-bit types

inline uint16_t byteswap16(uint16_t u)
{
   uint16_t high = u >> 8;
   uint16_t low = u & 0xFF;

   return high | (low << 8);
}

inline uint32_t byteswap32(uint32_t u)
{
   uint32_t high = u >> 16;
   uint32_t low = u & 0xFFFF;

   return byteswap16(high) | (uint32_t(byteswap16(low) ) << 16);
}

inline uint64_t byteswap64(uint64_t u)
{
   uint64_t high = u >> 32;
   uint64_t low = u & 0xFFFFFFFF;

   return byteswap32(high) | (uint64_t(byteswap32(low) ) << 32);
}

#if BYTE_ORDER == BIG_ENDIAN // BIG_ENDIAN

   #define HOST_TO_LE_16(value) (::byteswap16(value) )
   #define HOST_TO_LE_32(value) (::byteswap32(value) )
   #define HOST_TO_LE_64(value) (::byteswap64(value) )

#else // LITTLE_ENDIAN

   #define HOST_TO_LE_16(value) (value)
   #define HOST_TO_LE_32(value) (value)
   #define HOST_TO_LE_64(value) (value)

#endif // BYTE_ORDER


#define LE_TO_HOST_16(value)    HOST_TO_LE_16(value)
#define LE_TO_HOST_32(value)    HOST_TO_LE_32(value)
#define LE_TO_HOST_64(value)    HOST_TO_LE_64(value)


#endif /* BYTESWAP_H_ */
