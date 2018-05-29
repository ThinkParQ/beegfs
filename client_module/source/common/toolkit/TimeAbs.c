#include <common/toolkit/TimeAbs.h>
#include <common/Common.h>


void TimeAbs_init(TimeAbs* this)
{
   struct timeval now;
   do_gettimeofday(&now);

   this->now.tv_sec = now.tv_sec;
   this->now.tv_usec = now.tv_usec;
}

void TimeAbs_initFromOther(TimeAbs* this, TimeAbs* other)
{
   this->now = other->now;
}

void TimeAbs_uninit(TimeAbs* this)
{
}

void TimeAbs_setToNow(TimeAbs* this)
{
   struct timeval now;
   do_gettimeofday(&now);

   this->now.tv_sec = now.tv_sec;
   this->now.tv_usec = now.tv_usec;
}

