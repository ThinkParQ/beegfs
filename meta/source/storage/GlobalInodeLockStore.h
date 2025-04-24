#pragma once

#include <common/Common.h>
#include <storage/FileInode.h>

typedef std::map<std::string, FileInode*> GlobalInodeLockMap;
typedef GlobalInodeLockMap::iterator GlobalInodeLockMapIter;

typedef std::map<std::string, float> GlobalInodeTimestepMap; 

/**
 * Global store for file inodes which are locked for referencing. 
 * Used for chunk balancing operations. 
 * This object initalizes the GlobalInodeLockMap
 * It is used for taking and releasing locks on the inodes of file chunks.
 * * "locking" here means we successfully insert an element into a map. 
 */
class GlobalInodeLockStore
{
   friend class InodeFileStore;
   
   public:
      ~GlobalInodeLockStore()
      {
         this->clearLockStore();
      }

      bool insertFileInode(EntryInfo* entryInfo);
      bool releaseFileInode(const std::string& entryID); 
      bool lookupFileInode(EntryInfo* entryInfo); 
   private:
      GlobalInodeLockMap inodes;
      GlobalInodeTimestepMap inodeTimes;
      FileInode* getFileInode(EntryInfo* entryInfo);
      bool releaseInodeTime(const std::string& entryID);     

      RWLock rwlock;
      void clearLockStore(); 
      void clearTimeStoreUnlocked(); 
};


