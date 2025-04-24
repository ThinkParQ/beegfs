#include <common/threading/UniqueRWLock.h>
#include <common/threading/RWLockGuard.h>
#include <program/Program.h>
#include "GlobalInodeLockStore.h"


/**
 * Note: remember to call releaseFileInode()
 *
 * @return false of file is already in store or inode creation failed, true if file was inserted
 */
bool GlobalInodeLockStore::insertFileInode(EntryInfo* entryInfo)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   std::string entryID = entryInfo->getEntryID();
   GlobalInodeLockMapIter iter =  this->inodes.find(entryID);

   if(iter == this->inodes.end())
   { // not in map yet => try to insert it in map
      LOG_DBG(GENERAL, SPAM, "Insert file inode in GlobalInodeLockStore.", ("FileInodeID", iter->first));
      FileInode* inode = FileInode::createFromEntryInfo(entryInfo);
      if(!inode)
         return false;
      this->inodes.insert(GlobalInodeLockMap::value_type(entryID, inode));
      this->inodeTimes.insert(GlobalInodeTimestepMap::value_type(entryID, 0.0));
      return true;
   }
   return false;
}

/**
 * @return false if file was not in file or time store, true if we found the file in the file and time store
 */
bool GlobalInodeLockStore::releaseFileInode(const std::string& entryID)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   GlobalInodeLockMapIter iter =  this->inodes.find(entryID);

   if(iter != this->inodes.end() )
   { // inode is in the map, release it
      LOG_DBG(GENERAL, SPAM, "Release file inode in GlobalInodeLockStore.", ("FileInodeID", iter->first));
      delete(iter->second);
      this->inodes.erase(iter);
      return this->releaseInodeTime(entryID);
   }
   return false;
}

/**
 * Note: SafeRWLock_WRITE lock should be held here
 * 
 * @return false if file was not in time store at all, true if we found the file in the time store
 */
bool GlobalInodeLockStore::releaseInodeTime(const std::string& entryID)
{
   GlobalInodeTimestepMap::iterator iterTime = this->inodeTimes.find(entryID);
   if(iterTime != this->inodeTimes.end() )
   {
      this->inodeTimes.erase(iterTime);
      return true;
   }
   return false;
}

/**
 * @return true if file is in the store, false if it is not
 */
bool GlobalInodeLockStore::lookupFileInode(EntryInfo* entryInfo) 
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   auto iter = this->inodes.find(entryInfo->getEntryID());
   return (iter != this->inodes.end());
}

FileInode* GlobalInodeLockStore::getFileInode(EntryInfo* entryInfo)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);
   std::string entryID = entryInfo->getEntryID();
   GlobalInodeLockMapIter iter =  this->inodes.find(entryID);
   
   if(iter != this->inodes.end() )
   { 
      FileInode* inode = iter->second;
      return inode;
   }
   return nullptr;
}

void GlobalInodeLockStore::clearLockStore()
{
   LOG_DBG(GENERAL, DEBUG, "GlobalInodeLockStore::clearLockStore",
         ("# of loaded entries to be cleared", inodes.size()));

   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   for(GlobalInodeLockMapIter iter = this->inodes.begin(); iter != this->inodes.end();)
   {
      delete(iter->second);
      iter = this->inodes.erase(iter); // return next iterator after erase and assign to `iter` variable
   }
   //clear Timestep store
   this->clearTimeStoreUnlocked();
}

void GlobalInodeLockStore::clearTimeStoreUnlocked()
{
   for(GlobalInodeTimestepMap::iterator iter = this->inodeTimes.begin(); iter != this->inodeTimes.end();)
   {
      iter = this->inodeTimes.erase(iter); // return next iterator after erase and assign to `iter` variable
   }
}


