#ifndef COMMON_UNIQUERWLOCK_H
#define COMMON_UNIQUERWLOCK_H

#include <common/threading/RWLock.h>
#include <common/threading/SafeRWLock.h>

class UniqueRWLock
{
   public:
      UniqueRWLock()
         : rwlock(nullptr), locked(false)
      { }


      UniqueRWLock(RWLock& lock, SafeRWLockType type)
         : rwlock(&lock), locked(true)
      {
         if (type == SafeRWLock_READ)
            lock.readLock();
         else
            lock.writeLock();
      }

      ~UniqueRWLock()
      {
         if (locked)
            rwlock->unlock();
      }

      UniqueRWLock(UniqueRWLock&& old)
         : rwlock(nullptr), locked(false)
      {
         swap(old);
      }

      UniqueRWLock& operator=(UniqueRWLock&& other)
      {
         UniqueRWLock(std::move(other)).swap(*this);
         return *this;
      }

      UniqueRWLock(const UniqueRWLock&) = delete;
      UniqueRWLock& operator=(const UniqueRWLock&) = delete;

      void unlock()
      {
         rwlock->unlock();
         locked = false;
      }

      void lock(SafeRWLockType type)
      {
         if (type == SafeRWLock_READ)
            rwlock->readLock();
         else
            rwlock->writeLock();

         locked = true;
      }

      void swap(UniqueRWLock& other)
      {
         std::swap(rwlock, other.rwlock);
         std::swap(locked, other.locked);
      }

   private:
      RWLock* rwlock;
      bool locked;
};

inline void swap(UniqueRWLock& a, UniqueRWLock& b)
{
   a.swap(b);
}

#endif
