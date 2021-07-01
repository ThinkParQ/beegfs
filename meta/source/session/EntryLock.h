#ifndef ENTRYLOCK_H_
#define ENTRYLOCK_H_

#include "EntryLockStore.h"

template<typename LockDataT>
class UniqueEntryLockBase
{
   public:
      ~UniqueEntryLockBase()
      {
         if (lockData)
            entryLockStore->unlock(lockData);
      }

      UniqueEntryLockBase(const UniqueEntryLockBase&) = delete;
      UniqueEntryLockBase& operator=(const UniqueEntryLockBase&) = delete;

      UniqueEntryLockBase(UniqueEntryLockBase&& src)
         : entryLockStore(NULL), lockData(NULL)
      {
         swap(src);
      }

      UniqueEntryLockBase& operator=(UniqueEntryLockBase&& src)
      {
         UniqueEntryLockBase(std::move(src)).swap(*this);
         return *this;
      }

      void swap(UniqueEntryLockBase& other)
      {
         std::swap(entryLockStore, other.entryLockStore);
         std::swap(lockData, other.lockData);
      }

   protected:
      typedef UniqueEntryLockBase BaseType;

      template<typename... ArgsT>
      UniqueEntryLockBase(EntryLockStore* entryLockStore, const ArgsT&... args)
         : entryLockStore(entryLockStore)
      {
         lockData = entryLockStore->lock(args...);
      }

      UniqueEntryLockBase()
         : entryLockStore(NULL), lockData(NULL)
      {
      }

   private:
      EntryLockStore* entryLockStore;
      LockDataT* lockData;
};

template<typename LockDataT>
inline void swap(UniqueEntryLockBase<LockDataT>& a, UniqueEntryLockBase<LockDataT>& b)
{
   a.swap(b);
}



class FileIDLock : UniqueEntryLockBase<FileIDLockData>
{
   public:
      FileIDLock() = default;

      FileIDLock(const FileIDLock&) = delete;
      FileIDLock& operator=(const FileIDLock&) = delete;

      FileIDLock(FileIDLock&& src) : BaseType(std::move(src)) {}
      FileIDLock& operator=(FileIDLock&& src)
      {
         BaseType::operator=(std::move(src));
         return *this;
      }

      FileIDLock(EntryLockStore* entryLockStore, const std::string& fileID, const bool writeLock)
         : UniqueEntryLockBase<FileIDLockData>(entryLockStore, fileID, writeLock)
      {
      }
};

class ParentNameLock : UniqueEntryLockBase<ParentNameLockData>
{
   public:
      ParentNameLock() = default;

      ParentNameLock(const ParentNameLock&) = delete;
      ParentNameLock& operator=(const ParentNameLock&) = delete;

      ParentNameLock(ParentNameLock&& src) : BaseType(std::move(src)) {}
      ParentNameLock& operator=(ParentNameLock&& src)
      {
         BaseType::operator=(std::move(src));
         return *this;
      }

      ParentNameLock(EntryLockStore* entryLockStore, const std::string& parentID,
            const std::string& name)
         : UniqueEntryLockBase<ParentNameLockData>(entryLockStore, parentID, name)
      {
      }
};

class HashDirLock : UniqueEntryLockBase<HashDirLockData>
{
   public:
      HashDirLock() = default;

      HashDirLock(const HashDirLock&) = delete;
      HashDirLock& operator=(const HashDirLock&) = delete;

      HashDirLock(HashDirLock&& src) : BaseType(std::move(src)) {}
      HashDirLock& operator=(HashDirLock&& src)
      {
         BaseType::operator=(std::move(src));
         return *this;
      }

      HashDirLock(EntryLockStore* entryLockStore, std::pair<unsigned, unsigned> hashDir)
         : UniqueEntryLockBase<HashDirLockData>(entryLockStore, hashDir)
      {
      }
};

#endif /* ENTRYLOCK_H_ */
