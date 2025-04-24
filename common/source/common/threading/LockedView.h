#pragma once

#include <mutex>
#include <utility>

template<typename T>
class LockedView
{
   T *mPtr = nullptr;
   std::unique_lock<std::mutex> mLock;

public:

   T *operator->() const
   {
      return mPtr;
   }

   std::unique_lock<std::mutex>& get_unique_lock()
   {
      return mLock;
   }

   LockedView(LockedView const& other) = delete;

   LockedView(LockedView&& other)
   {
      std::swap(mPtr, other.mPtr);
      std::swap(mLock, other.mLock);
   }

   LockedView() = delete;

   LockedView(T *ptr, std::mutex *mutex)
      : mPtr(ptr), mLock(*mutex)
   {}
};

template<typename T>
class MutexProtected
{
   T value;
   std::mutex mutex;

public:

   LockedView<T> lockedView()
   {
      return LockedView(&value, &mutex);
   }
};
