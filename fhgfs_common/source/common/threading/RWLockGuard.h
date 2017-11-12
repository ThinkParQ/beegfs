#ifndef COMMON_RWLOCKGUARD_H
#define COMMON_RWLOCKGUARD_H

#include <common/threading/RWLock.h>
#include <common/threading/SafeRWLock.h>

class RWLockGuard
{
   public:
      RWLockGuard(RWLock& lock, SafeRWLockType type)
         : lock(&lock)
      {
         if (type == SafeRWLock_READ)
            lock.readLock();
         else
            lock.writeLock();
      }

      ~RWLockGuard()
      {
         lock->unlock();
      }

      RWLockGuard(const RWLockGuard&) = delete;
      RWLockGuard(RWLockGuard&&) = delete;
      RWLockGuard& operator=(const RWLockGuard&) = delete;
      RWLockGuard& operator=(RWLockGuard&&) = delete;

   private:
      RWLock* lock;
};

#endif
