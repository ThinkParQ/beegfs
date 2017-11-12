#ifndef TIMETK_H_
#define TIMETK_H_

#include <common/Common.h>

#include <linux/sched.h>


/*
 * Max number of seconds that the scheduler can wait for
 */
#define TIMETK_MAX_SCHEDULE_SECONDS \
   ( (unsigned long) (MAX_SCHEDULE_TIMEOUT / HZ)-1)
/*
 * Round x up to the next unit y (used by timeval to jiffies translation)
 */
#define TIMETK_ROUND_UP(x,y) (((x)+(y)-1)/(y))

static inline void TimeTk_jiffiesToTimeval(uint64_t jiffiesValue, struct timeval* outTV);
static inline unsigned TimeTk_jiffiesToMS(unsigned long jiffiesValue);
static inline long TimeTk_timevalToJiffies(struct timeval* tvp);
static inline long TimeTk_timevalToJiffiesSchedulable(struct timeval* tvp);
static inline long TimeTk_msToJiffiesSchedulable(unsigned ms);



void TimeTk_jiffiesToTimeval(uint64_t jiffiesValue, struct timeval* outTV)
{
   jiffies_to_timeval(jiffiesValue, outTV);
}

unsigned TimeTk_jiffiesToMS(unsigned long jiffiesValue)
{
   return jiffies_to_msecs(jiffiesValue);
}


/*
 * Translate timeval to jiffies
 */
long TimeTk_timevalToJiffies(struct timeval* tvp)
{
   return (tvp ? (long)timeval_to_jiffies(tvp) : ( (long)(~0) >> 1) );
}

/*
 * Translate timeval to jiffies.
 * Note: Limits timeval to MAX_SCHEDULE_TIMEOUT
 */
long TimeTk_timevalToJiffiesSchedulable(struct timeval* tvp)
{
   return (tvp && ( (unsigned long)(tvp->tv_sec) < TIMETK_MAX_SCHEDULE_SECONDS) ?
      timeval_to_jiffies(tvp) : MAX_SCHEDULE_TIMEOUT);
}

long TimeTk_msToJiffiesSchedulable(unsigned ms)
{
   unsigned long resultJiffies = msecs_to_jiffies(ms);

   return (resultJiffies >= MAX_SCHEDULE_TIMEOUT) ? (MAX_SCHEDULE_TIMEOUT-1) : resultJiffies;
}


#endif /*TIMETK_H_*/
