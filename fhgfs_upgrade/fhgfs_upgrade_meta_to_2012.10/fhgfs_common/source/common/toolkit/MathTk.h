#ifndef MATHTK_H_
#define MATHTK_H_

#include <common/Common.h>


class MathTk
{
   public:


   private:
      MathTk() {};


   public:
      // inliners

      /**
       * Base 2 logarithm.
       *
       * Note: This is intended to optimize division, e.g. "a=b/c" becomes "a=b>>log2(c)" if c is
       * guaranteed to be a 2^n number.
       *
       * @param value may not be 0
       * @return log2 of value
       */
      static unsigned log2Int64(uint64_t value)
      {
         /* __builtin_clz: Count leading zeros - returns the number of leading 0-bits in x, starting
          * at the most significant bit position. If x is 0, the result is undefined. */
         // (note: 8 is bits_per_byte)
         unsigned result = (sizeof(value) * 8) - 1 - __builtin_clzll(value);

         return result;
      }

      /**
       * Base 2 logarithm.
       *
       * See log2Int64() for details.
       */
      static unsigned log2Int32(unsigned value)
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
      static bool isPowerOfTwo(unsigned value)
      {
         /* note: there is also a __builtin_popcount() function, which could be used for this;
            but that function is for some reason really slow (as slow as division) and thus is not
            an option here, as this method is intented to speed up code paths by avoiding
            division. */

         /* note: builtin_clz/ctz (count leading/trailing zeros) are undefined if value has no bits
            set. thus, this should not be called with value==0, but we don't want to test that in
            here because this is used in performance-critical paths. */

         int numValueZeroBits = __builtin_clz(value) + __builtin_ctz(value);
         int numPowerTwoZeroBits = (8 * sizeof(value) ) - 1;

         return (numValueZeroBits == numPowerTwoZeroBits);
      }

};


#endif /* MATHTK_H_ */
