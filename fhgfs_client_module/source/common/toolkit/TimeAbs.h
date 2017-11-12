#ifndef OPEN_TIMEABS_H_
#define OPEN_TIMEABS_H_

#include <common/Common.h>


struct TimeAbs;
typedef struct TimeAbs TimeAbs;


extern void TimeAbs_init(TimeAbs* this);
extern void TimeAbs_initFromOther(TimeAbs* this, TimeAbs* other);
extern void TimeAbs_uninit(TimeAbs* this);

extern void TimeAbs_setToNow(TimeAbs* this);

// inliners
static inline void TimeAbs_setFromOther(TimeAbs* this, TimeAbs* other);
static inline unsigned TimeAbs_elapsedSinceMS(TimeAbs* this, TimeAbs* earlierT);
static inline unsigned TimeAbs_elapsedSinceMicro(TimeAbs* this, TimeAbs* earlierT);
static inline unsigned TimeAbs_elapsedMS(TimeAbs* this);

// getters & setters
static inline struct timeval* TimeAbs_getTimeval(TimeAbs* this);

// operators
static inline bool TimeAbs_equals(TimeAbs* this, TimeAbs* other);
static inline bool TimeAbs_notequals(TimeAbs* this, TimeAbs* other);


/**
 * This time class is based on a non-monotonic clock. Use the Time class instead of this one,
 * whenever possible.
 */
struct TimeAbs
{
   struct timeval now;
};



void TimeAbs_setFromOther(TimeAbs* this, TimeAbs* other)
{
   this->now = other->now;
}

unsigned TimeAbs_elapsedSinceMS(TimeAbs* this, TimeAbs* earlierT)
{
   unsigned secs = (this->now.tv_sec - earlierT->now.tv_sec) * 1000;
   int micros = (this->now.tv_usec - earlierT->now.tv_usec) / 1000;

   return secs + micros;
}

unsigned TimeAbs_elapsedSinceMicro(TimeAbs* this, TimeAbs* earlierT)
{
   unsigned secs = (this->now.tv_sec - earlierT->now.tv_sec) * 1000000;
   int micros = (this->now.tv_usec - earlierT->now.tv_usec);

   return secs + micros;
}

unsigned TimeAbs_elapsedMS(TimeAbs* this)
{
   unsigned elapsedMS;

   TimeAbs currentT;
   TimeAbs_init(&currentT);

   elapsedMS = TimeAbs_elapsedSinceMS(&currentT, this);

   TimeAbs_uninit(&currentT);

   return elapsedMS;
}

struct timeval* TimeAbs_getTimeval(TimeAbs* this)
{
   return &this->now;
}

bool TimeAbs_equals(TimeAbs* this, TimeAbs* other)
{
   return (this->now.tv_usec == other->now.tv_usec) && (this->now.tv_sec == other->now.tv_sec);
}

bool TimeAbs_notequals(TimeAbs* this, TimeAbs* other)
{
   return (this->now.tv_usec != other->now.tv_usec) || (this->now.tv_sec != other->now.tv_sec);
}


#endif /*OPEN_TIMEABS_H_*/
