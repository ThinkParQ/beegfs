#ifndef OPEN_RWLOCK_H_
#define OPEN_RWLOCK_H_

#include <common/Common.h>

#include <linux/rwsem.h>


struct RWLock;
typedef struct RWLock RWLock;


static inline void RWLock_init(RWLock* this);
static inline void RWLock_writeLock(RWLock* this);
static inline int RWLock_writeTryLock(RWLock* this);
static inline void RWLock_readLock(RWLock* this);
static inline void RWLock_writeUnlock(RWLock* this);
static inline void RWLock_readUnlock(RWLock* this);


struct RWLock
{
   struct rw_semaphore rwSem;
};


void RWLock_init(RWLock* this)
{
   init_rwsem(&this->rwSem);
}

void RWLock_writeLock(RWLock* this)
{
   down_write(&this->rwSem);
}

/**
 * Try locking and return immediately even if lock cannot be aqcuired immediately.
 *
 * @return 1 if lock acquired, 0 if contention
 */
int RWLock_writeTryLock(RWLock* this)
{
   return down_write_trylock(&this->rwSem);
}

void RWLock_readLock(RWLock* this)
{
   down_read(&this->rwSem);
}

void RWLock_writeUnlock(RWLock* this)
{
   up_write(&this->rwSem);
}

void RWLock_readUnlock(RWLock* this)
{
   up_read(&this->rwSem);
}



#endif /* OPEN_RWLOCK_H_ */
