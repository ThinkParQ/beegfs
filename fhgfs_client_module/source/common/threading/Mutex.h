#ifndef OPEN_MUTEX_H_
#define OPEN_MUTEX_H_

#include <common/Common.h>

#include <linux/mutex.h>


struct Mutex;
typedef struct Mutex Mutex;

static inline void Mutex_init(Mutex* this);
static inline void Mutex_uninit(Mutex* this);
static inline void Mutex_lock(Mutex* this);
static inline void Mutex_unlock(Mutex* this);


struct Mutex
{
   struct mutex mutex;
};


void Mutex_init(Mutex* this)
{
   mutex_init(&this->mutex);
}

void Mutex_uninit(Mutex* this)
{
   mutex_destroy(&this->mutex); /* optional call according to kernel api description, so we might
      just drop it if necessary (however, it has some functionality regarding to kernel mutex
      debugging) */
}

void Mutex_lock(Mutex* this)
{
   mutex_lock(&this->mutex);
}

void Mutex_unlock(Mutex* this)
{
   mutex_unlock(&this->mutex);
}


#endif /*OPEN_MUTEX_H_*/
