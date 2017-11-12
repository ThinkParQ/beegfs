#ifndef RANDOMREENTRANT_H_
#define RANDOMREENTRANT_H_

#include <common/Common.h>

/*
 * Note: This is not perfectly reentrant in the sense that it would use a mutex or atomic ops
 * for seed updates, but it is safe for multi-threaded use and tries to make it unlikely that two
 * simultaneous threads generate random numbers based on the same seed.
 */

class RandomReentrant
{
   public:
      RandomReentrant()
      {
         struct timeval tv;
         gettimeofday(&tv, NULL);

         seed = tv.tv_usec;
      }

      RandomReentrant(unsigned seed)
      {
         this->seed = seed;
      }

      /**
       * @return positive (incl. zero) random number
       */
      int getNextInt()
      {
         unsigned seedTmp = seed++; /* "++" to make it unlikely that another thread uses the same
            seed */

         int randVal = rand_r(&seedTmp);

         seed = seedTmp;

         // Note: -randVal (instead of ~randVal) wouldn't work for INT_MIN

         return (randVal < 0) ? ~randVal : randVal;
      }

      /**
       * @param min inclusive min value
       * @param max inclusive max value
       */
      int getNextInRange(int min, int max)
      {
         int randVal = getNextInt() % (max - min + 1);

         return(min + randVal);
      }

   private:
      volatile unsigned seed;
};

#endif /* RANDOMREENTRANT_H_ */
