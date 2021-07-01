#ifndef ENTRYLOCKSTORE_H_
#define ENTRYLOCKSTORE_H_

#include <common/Common.h>
#include <common/app/log/LogContext.h>
#include <common/threading/Mutex.h>
#include <common/threading/RWLock.h>

template<typename Value>
struct ValueLockHash;



// This class implements a hashmap Value -> Lock of size HashSize. To avoid memory allocations
// and deallocations, each bucket may keep up to BufferSize lock objects that are not currently
// used.
//
// Hashes for values are computed by ValueLockHash<Value>, so an appropriate specialization of this
// template must exists in order to use this class.
//
// Currently the Lock argument is not restricted in any way, but it is intended to only ever be set
// to types of locking primitives.
//
// This implementation does not use boost::unordered_map because we want a fixed-size table and
// control over the per-bucket value lists, since these lists required memory allocations and
// insert and deallocations on erase. The allocation overhead per list entry could also be
// optimized away by using intrusive lists for bucket contents, but currently the lock buffers
// are sufficient to amortize almost all allocator traffic.
template<typename Value, typename Lock, unsigned HashSize, unsigned BufferSize = 32>
class ValueLockStore
{
   private:
      struct LockBucket;

   public:
      class ValueLock
      {
         friend class ValueLockStore;

         public:
            Lock& getLock()
            {
               return lock;
            }

         private:
            Lock lock;
            Value value;
            LockBucket* bucket;
            typename std::list<ValueLock*>::iterator iter;
            unsigned references;

            ValueLock(const Value& value, LockBucket& bucket,
                  typename std::list<ValueLock*>::iterator iter)
               : value(value), bucket(&bucket), iter(iter), references(0)
            {}

            ValueLock(const ValueLock&);
            ValueLock& operator=(const ValueLock&);
      };

   private:
      struct LockBucket
      {
         Mutex mtx;

         std::list<ValueLock*> locks;

         std::list<ValueLock*> lockBuffer;
         unsigned lockBufferSize;

         LockBucket()
            : lockBufferSize(0)
         {}
      };

   public:
      // Acquires a lock descriptor for `value`. The descriptor is only acquired, not locked;
      // locking and unlocking is left to the user. Increments the reference count of the returned
      // descriptor. The descriptor must be released with `putLock` when done.
      ValueLock& getLockFor(const Value& value)
      {
         const uint32_t valueHash = ValueLockHash<Value>()(value);
         LockBucket& bucket = buckets[valueHash % HashSize];

         ValueLock* lock = NULL;

         bucket.mtx.lock();
         {
            for(typename std::list<ValueLock*>::iterator it = bucket.locks.begin(),
                     end = bucket.locks.end(); it != end; ++it)
            {
               if(value == (*it)->value)
               {
                  lock = *it;
                  break;
               }
            }

            if(!lock)
            {
               if(bucket.lockBufferSize == 0)
               {
                  bucket.locks.push_front(NULL);
                  lock = new ValueLock(value, bucket, bucket.locks.begin() );
                  bucket.locks.front() = lock;
               }
               else
               {
                  bucket.locks.splice(
                     bucket.locks.begin(),
                     bucket.lockBuffer,
                     bucket.lockBuffer.begin() );
                  bucket.lockBufferSize--;

                  lock = bucket.locks.front();
                  lock->value = value;
               }
            }

            lock->references++;
         }
         bucket.mtx.unlock();

         return *lock;
      }

      // Releases a lock descriptor acquired by `getLockFor` and decreases its reference counter.
      // When the reference count reaches 0, `lock` is invalidated.
      void putLock(ValueLock& lock)
      {
         LockBucket& bucket = *lock.bucket;

         bucket.mtx.lock();
         do {
            lock.references--;

            if(lock.references > 0)
               break;

            if(bucket.lockBufferSize < BufferSize)
            {
               bucket.lockBuffer.splice(
                  bucket.lockBuffer.begin(),
                  bucket.locks,
                  lock.iter);
               bucket.lockBufferSize++;
            }
            else
            {
               bucket.locks.erase(lock.iter);
               delete &lock;
            }
         } while (0);
         bucket.mtx.unlock();
      }

   private:
      LockBucket buckets[HashSize];
};



template<>
struct ValueLockHash<std::string>
{
   // this is the jenkins hash function (http://www.burtleburtle.net/bob/hash/doobs.html).
   // it was chosen for this use case because it is fast for small inputs, has no alignment
   // requirements and exhibits good mixing.
   uint32_t operator()(const std::string& str) const
   {
      uint32_t hash, i;
      for(hash = i = 0; i < str.size(); ++i)
      {
         hash += str[i];
         hash += (hash << 10);
         hash ^= (hash >> 6);
      }
      hash += (hash << 3);
      hash ^= (hash >> 11);
      hash += (hash << 15);
      return hash;
   }
};

template<>
struct ValueLockHash<std::pair<std::string, std::string> >
{
   uint32_t operator()(std::pair<const std::string&, const std::string&> pair) const
   {
      ValueLockHash<std::string> hash;
      return hash(pair.first) ^ hash(pair.second);
   }
};

template<>
struct ValueLockHash<std::pair<unsigned, unsigned>>
{
   uint32_t operator()(std::pair<unsigned, unsigned> pair) const
   {
      // optimized for the inode/dentries hash dir structure of 2 fragments of 7 bits each
      return std::hash<unsigned>()((pair.first << 8) | pair.second);
   }
};

typedef ValueLockStore<std::pair<std::string, std::string>, Mutex, 1024> ParentNameLockStore;
typedef ParentNameLockStore::ValueLock ParentNameLockData;

typedef ValueLockStore<std::string, RWLock, 1024> FileIDLockStore;
typedef FileIDLockStore::ValueLock FileIDLockData;

typedef ValueLockStore<std::pair<unsigned, unsigned>, Mutex, 1024> HashDirLockStore;
typedef HashDirLockStore::ValueLock HashDirLockData;

class EntryLockStore
{
   public:
      ParentNameLockData* lock(const std::string& parentID, const std::string& name);
      //FileIDLock is used for both files and directories
      FileIDLockData* lock(const std::string& fileID, const bool writeLock);
      HashDirLockData* lock(std::pair<unsigned, unsigned> hashDir);

      void unlock(ParentNameLockData* parentNameLockData);
      void unlock(FileIDLockData* fileIDLockData);
      void unlock(HashDirLockData* hashDirLockData);

   private:
      ParentNameLockStore parentNameLocks;
      FileIDLockStore fileLocks;
      HashDirLockStore hashDirLocks;
};

#endif /* ENTRYLOCKSTORE_H_ */
