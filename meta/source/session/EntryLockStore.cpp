#include "EntryLockStore.h"

ParentNameLockData* EntryLockStore::lock(const std::string& parentID, const std::string& name)
{
   ParentNameLockData& lock = parentNameLocks.getLockFor(
      std::pair<const std::string&, const std::string&>(parentID, name) );
   lock.getLock().lock();
   return &lock;
}

FileIDLockData* EntryLockStore::lock(const std::string& fileID, const bool writeLock)
{
   FileIDLockData& lock = fileLocks.getLockFor(fileID);
   if(writeLock)
      lock.getLock().writeLock();
   else
      lock.getLock().readLock();

   return &lock;
}

HashDirLockData* EntryLockStore::lock(std::pair<unsigned, unsigned> hashDir)
{
   HashDirLockData& lock = hashDirLocks.getLockFor(hashDir);
   lock.getLock().lock();
   return &lock;
}

void EntryLockStore::unlock(ParentNameLockData* parentNameLockData)
{
   parentNameLockData->getLock().unlock();
   parentNameLocks.putLock(*parentNameLockData);
}

void EntryLockStore::unlock(FileIDLockData* fileIDLockData)
{
   fileIDLockData->getLock().unlock();
   fileLocks.putLock(*fileIDLockData);
}

void EntryLockStore::unlock(HashDirLockData* hashDirLockData)
{
   hashDirLockData->getLock().unlock();
   hashDirLocks.putLock(*hashDirLockData);
}
