#ifndef RANDOM_H_
#define RANDOM_H_

#include <common/Common.h>

class Random
{
   public:
      Random()
      {
         struct timeval tv;
         gettimeofday(&tv, NULL);

         seed = tv.tv_usec;
      }

      Random(unsigned seed)
      {
         this->seed = seed;
      }

      /**
       * @return positive (incl. zero) random number
       */
      int getNextInt()
      {
         int randVal = rand_r(&seed);

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
      unsigned seed;
};

#endif /*RANDOM_H_*/
