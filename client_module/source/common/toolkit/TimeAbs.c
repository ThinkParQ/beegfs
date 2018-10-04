#include <common/toolkit/TimeAbs.h>
#include <common/Common.h>


void TimeAbs_init(TimeAbs* this)
{
   struct timeval now;
   do_gettimeofday(&now);

   this->now.tv_sec = now.tv_sec;
   this->now.tv_usec = now.tv_usec;
}
