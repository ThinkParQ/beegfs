#include <common/toolkit/Random.h>
#include <common/Common.h>

#include <linux/random.h>


/**
 * @return positive (incl. zero) random number
 */
int Random_getNextInt(void)
{
   int randVal;

   get_random_bytes(&randVal, sizeof(randVal) );

   // Note: -randVal (instead of ~randVal) wouldn't work for INT_MIN

   return (randVal < 0) ? (~randVal) : randVal;
}

/**
 * @param min inclusive min value
 * @param max inclusive max value
 */
int Random_getNextInRange(int min, int max)
{
   int randVal = Random_getNextInt() % (max - min + 1);

   return(min + randVal);
}

