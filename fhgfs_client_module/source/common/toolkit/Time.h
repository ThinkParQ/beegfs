#ifndef OPEN_TIME_H_
#define OPEN_TIME_H_

#include <common/Common.h>

#include <linux/ktime.h>


struct Time;
typedef struct Time Time;


static inline void Time_init(Time* this);
static inline void Time_initZero(Time* this);
static inline void Time_initFromOther(Time* this, Time* other);
static inline void Time_uninit(Time* this);

static inline void Time_setToNow(Time* this);
static inline void Time_setZero(Time* this);
static inline bool Time_getIsZero(Time* this);

static inline void Time_setFromOther(Time* this, Time* other);
static inline unsigned Time_elapsedSinceMS(Time* this, Time* earlierT);
static inline unsigned Time_elapsedSinceMicro(Time* this, Time* earlierT);
static inline unsigned Time_elapsedMS(Time* this);

// operators
static inline bool Time_equals(Time* this, Time* other);
static inline bool Time_notequals(Time* this, Time* other);


/**
 * The basis for this Time class is a monotonic clock. Use it whenever possible instead of the
 * TimeAbs class.
 */
struct Time
{
   struct timespec now;
};


/**
 * Set to current time.
 */
void Time_init(Time* this)
{
   ktime_get_ts(&this->now);
}

/**
 * Set time values to zero, e.g. to mark this time as unitialized or far back in the past.
 */
void Time_initZero(Time* this)
{
   Time_setZero(this);
}

void Time_initFromOther(Time* this, Time* other)
{
   this->now = other->now;
}

void Time_uninit(Time* this)
{
}


void Time_setToNow(Time* this)
{
   ktime_get_ts(&this->now);
}


void Time_setZero(Time* this)
{
   this->now.tv_sec = 0;
   this->now.tv_nsec = 0;
}

bool Time_getIsZero(Time* this)
{
   return (!this->now.tv_sec && !this->now.tv_sec);
}

void Time_setFromOther(Time* this, Time* other)
{
   this->now = other->now;
}


unsigned Time_elapsedSinceMS(Time* this, Time* earlierT)
{
   unsigned secs = (this->now.tv_sec - earlierT->now.tv_sec) * 1000;
   int micros = (this->now.tv_nsec - earlierT->now.tv_nsec) / 1000000; /* can also be negative,
                                                                          so this must be signed */

   return secs + micros;
}


unsigned Time_elapsedSinceMicro(Time* this, Time* earlierT)
{
   unsigned secs = (this->now.tv_sec - earlierT->now.tv_sec) * 1000000;
   int micros = (this->now.tv_nsec - earlierT->now.tv_nsec) / 1000; /* can also be negative,
                                                                       so this must be signed */

   return secs + micros;
}


unsigned Time_elapsedMS(Time* this)
{
   unsigned elapsedMS;

   Time currentT;
   Time_init(&currentT);

   elapsedMS = Time_elapsedSinceMS(&currentT, this);

   Time_uninit(&currentT);

   return elapsedMS;
}


bool Time_equals(Time* this, Time* other)
{
   return (this->now.tv_nsec == other->now.tv_nsec) && (this->now.tv_sec == other->now.tv_sec);
}


bool Time_notequals(Time* this, Time* other)
{
   return (this->now.tv_nsec != other->now.tv_nsec) || (this->now.tv_sec != other->now.tv_sec);
}


#endif /* OPEN_TIME_H_ */
