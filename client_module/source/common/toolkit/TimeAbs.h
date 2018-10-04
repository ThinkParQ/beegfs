#ifndef OPEN_TIMEABS_H_
#define OPEN_TIMEABS_H_

#include <common/Common.h>


struct TimeAbs;
typedef struct TimeAbs TimeAbs;


extern void TimeAbs_init(TimeAbs* this);

// getters & setters
static inline struct timeval* TimeAbs_getTimeval(TimeAbs* this);

/**
 * This time class is based on a non-monotonic clock. Use the Time class instead of this one,
 * whenever possible.
 */
struct TimeAbs
{
   struct timeval now;
};


struct timeval* TimeAbs_getTimeval(TimeAbs* this)
{
   return &this->now;
}

#endif /*OPEN_TIMEABS_H_*/
