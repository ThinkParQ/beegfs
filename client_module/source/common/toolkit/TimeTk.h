#ifndef TIMETK_H_
#define TIMETK_H_

#include <common/Common.h>

#include <linux/sched.h>


static inline long TimeTk_msToJiffiesSchedulable(unsigned ms);



long TimeTk_msToJiffiesSchedulable(unsigned ms)
{
   unsigned long resultJiffies = msecs_to_jiffies(ms);

   return (resultJiffies >= MAX_SCHEDULE_TIMEOUT) ? (MAX_SCHEDULE_TIMEOUT-1) : resultJiffies;
}


#endif /*TIMETK_H_*/
