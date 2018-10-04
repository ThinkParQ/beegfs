#ifndef SYNCHRONIZEDCOUNTER_H_
#define SYNCHRONIZEDCOUNTER_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <asm/atomic.h>
#include <linux/completion.h>

struct SynchronizedCounter;
typedef struct SynchronizedCounter SynchronizedCounter;

static inline void SynchronizedCounter_init(SynchronizedCounter* this);

// inliners
static inline void SynchronizedCounter_waitForCount(SynchronizedCounter* this, int waitCount);
static inline void SynchronizedCounter_incCount(SynchronizedCounter* this);
static inline void SynchronizedCounter_incCountBy(SynchronizedCounter* this, int count);


struct SynchronizedCounter
{
   atomic_t count;

   struct completion barrier;
};


void SynchronizedCounter_init(SynchronizedCounter* this)
{
   atomic_set(&this->count, 0);
   init_completion(&this->barrier);
}

void SynchronizedCounter_waitForCount(SynchronizedCounter* this, int waitCount)
{
   SynchronizedCounter_incCountBy(this, -waitCount);
   wait_for_completion(&this->barrier);
}

void SynchronizedCounter_incCount(SynchronizedCounter* this)
{
   SynchronizedCounter_incCountBy(this, 1);
}

void SynchronizedCounter_incCountBy(SynchronizedCounter* this, int count)
{
   if (atomic_add_return(count, &this->count) == 0)
      complete(&this->barrier);
}

#endif /*SYNCHRONIZEDCOUNTER_H_*/
