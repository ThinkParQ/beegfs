#ifndef OPEN_TIME_H_
#define OPEN_TIME_H_

#include <common/Common.h>

#include <linux/ktime.h>
#include <linux/time.h>



/**
 * The basis for this Time class is a monotonic clock.
 */
#ifdef KERNEL_HAS_64BIT_TIMESTAMPS
typedef struct timespec64 Time;
#else
typedef struct timespec Time;
#endif


static inline unsigned Time_elapsedSinceMS(Time* this, Time* earlierT);
static inline unsigned long long Time_elapsedSinceNS(Time* this, Time* earlierT);
static inline unsigned Time_elapsedMS(Time* this);
static inline int Time_compare(Time* this, Time* o);
static inline unsigned long long Time_toNS(Time* this);

static inline void Time_setToNow(Time* this)
{
#ifdef KERNEL_HAS_KTIME_GET_TS64
   ktime_get_ts64(this);
#else
   ktime_get_ts(this);
#endif
}

/**
 * Set time to to "real" time, i.e. get the time from a non-monotonic clock.
 */
static inline void Time_setToNowReal(Time *this)
{
#ifdef KERNEL_HAS_KTIME_GET_REAL_TS64
   ktime_get_real_ts64(this);
#else
   ktime_get_real_ts(this);
#endif
}

static inline void Time_setZero(Time *this)
{
   this->tv_sec = 0;
   this->tv_nsec = 0;
}



/**
 * Set to current time.
 */
static inline void Time_init(Time* this)
{
   Time_setToNow(this);
}

/**
 * Set time values to zero, e.g. to mark this time as unitialized or far back in the past.
 */
static inline void Time_initZero(Time* this)
{
   Time_setZero(this);
}

static inline bool Time_getIsZero(Time* this)
{
   return (!this->tv_sec && !this->tv_sec);
}

static inline unsigned Time_elapsedSinceMS(Time* this, Time* earlierT)
{
   unsigned secs = (this->tv_sec - earlierT->tv_sec) * 1000;
   int micros = (this->tv_nsec - earlierT->tv_nsec) / 1000000; /* can also be negative,
                                                                          so this must be signed */

   return secs + micros;
}

unsigned long long Time_elapsedSinceNS(Time* this, Time* earlierT)
{
   unsigned long long secs = (this->tv_sec - earlierT->tv_sec) * 1000000000ull;
   long nanos = (this->tv_nsec - earlierT->tv_nsec); /* can also be negative,
                                                                so this must be signed */

   return secs + nanos;
}

unsigned long long Time_toNS(Time* this)
{
   return this->tv_sec * 1000000000ull + this->tv_nsec;
}

int Time_compare(Time* this, Time* o)
{
   long long diff = Time_toNS(this) - Time_toNS(o);
   if (diff < 0)
      return -1;
   else if (diff > 0)
      return 1;
   return 0;
}

unsigned Time_elapsedMS(Time* this)
{
   unsigned elapsedMS;

   Time currentT;
   Time_init(&currentT);

   elapsedMS = Time_elapsedSinceMS(&currentT, this);

   return elapsedMS;
}

#endif /* OPEN_TIME_H_ */
