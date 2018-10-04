#ifndef OPEN_MATHTK_H_
#define OPEN_MATHTK_H_

#include <common/Common.h>


static inline unsigned MathTk_log2Int32(unsigned value);
static inline bool MathTk_isPowerOfTwo(unsigned value);

/**
 * Base 2 logarithm.
 *
 * See log2Int64() for details.
 */
unsigned MathTk_log2Int32(unsigned value)
{
   /* __builtin_clz: Count leading zeros - returns the number of leading 0-bits in x, starting
    * at the most significant bit position. If x is 0, the result is undefined. */
   // (note: 8 is bits_per_byte)
   unsigned result = (sizeof(value) * 8) - 1 - __builtin_clz(value);

   return result;
}

/**
 * Checks whether there is only a single bit set in value, in which case value is a power of
 * two.
 *
 * @param value may not be 0 (result is undefined in that case).
 * @return true if only a single bit is set (=> value is a power of two), false otherwise
 */
bool MathTk_isPowerOfTwo(unsigned value)
{
   //return ( (x != 0) && !(x & (x - 1) ) ); // this version is compatible with value==0

   return !(value & (value - 1) );
}


#endif /* OPEN_MATHTK_H_ */
