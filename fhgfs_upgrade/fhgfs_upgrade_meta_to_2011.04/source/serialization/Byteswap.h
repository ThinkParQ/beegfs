/*
 * Big- and little-endian byte swapping.
 */

#ifndef BYTESWAP_H_
#define BYTESWAP_H_

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


#endif /* BYTESWAP_H_ */
