#ifndef DIRENTRYSTORE_H_
#define DIRENTRYSTORE_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/MetadataTk.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include "DirEntry.h"


struct ListIncExOutArgs
{
   StringList* outNames; /* required */
   IntList* outEntryTypes; /* optional (can be NULL if caller is not interested; contains
      DirEntryType stored as int to ease message serialization) */
   int64_t* outNewServerOffset; /* required */
};


class DirEntryStore
{
   friend class DirInode;
   friend class MetaStore;
   
   public:
      DirEntryStore();
      DirEntryStore(std::string parentID);
      
      FhgfsOpsErr makeEntry(DirEntry* entry);
      FhgfsOpsErr linkInodeToDir(std::string& inodePath, std::string &fileName);

      FhgfsOpsErr removeDir(std::string entryName, DirEntry** outDirEntry);
      FhgfsOpsErr unlinkDirEntry(std::string entryName, DirEntry* inEntry, bool unlinkEntryName,
         DirEntry** outEntry);
      FhgfsOpsErr unlinkDirEntryName(std::string entryName);
      
      FhgfsOpsErr renameEntry(std::string entryName, std::string newEntryName);

      void list(StringVector* outNames);
      FhgfsOpsErr listIncremental(int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames, StringList* outNames, int64_t* outNewServerOffset);
      FhgfsOpsErr listIncrementalEx(int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames, ListIncExOutArgs& outArgs);
      FhgfsOpsErr listIDFilesIncremental(int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames, ListIncExOutArgs& outArgs);

      bool exists(std::string entryName);

      DirEntryType getEntryInfo(std::string entryName, EntryInfo* outInfo,
         DentryInodeMeta* outInodeMetaData);
      FhgfsOpsErr setOwnerNodeID(std::string entryName, uint16_t ownerNode);

      DirEntry* dirEntryCreateFromFile(std::string entryName);

      static FhgfsOpsErr mkDentryStoreDir(std::string dirID);
      static bool rmDirEntryStoreDir(std::string& id);

   private:
      std::string parentID; // ID of the directory to which this store belongs
      std::string dirEntryPath; /* path to dirEntry, without the last element (fileName)
                                 * depends on parentID, so changes when parentID is set
                                 *
                                 * Note: For upgrade tool: Must only be used on Reads!
                                 * */

      std::string dirEntryNumIDPath; // for upgrade tool, for writes

      RWLock rwlock;
      
      FhgfsOpsErr makeEntryUnlocked(DirEntry* entry);
      FhgfsOpsErr linkInodeToDirUnlocked(std::string& inodePath, std::string &fileName);


      FhgfsOpsErr removeDirUnlocked(std::string entryName, DirEntry** outDirEntry);
      FhgfsOpsErr unlinkDirEntryUnlocked(std::string entryName, DirEntry* inEntry,
         bool unlinkEntryName, DirEntry** outEntry);
      FhgfsOpsErr unlinkDirEntryNameUnlocked(std::string entryName);

      bool existsUnlocked(std::string entryName);
      
      const std::string& getDirEntryPathUnlocked();

      
   public:
      // inliners

      /**
       * @return false if no link with this name exists
       */
      bool getDentry(std::string entryName, DirEntry& outEntry)
      {
         // note: the difference to getDirDentry/getFileDentry is that this works independent
            // of the link type

         bool exists = false;
         
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);
         
         exists = outEntry.loadFromFileName(getDirEntryPathUnlocked(), entryName);
         
         safeLock.unlock();
         
         return exists;
      }

      const std::string& getParentEntryID(void)
      {
         return this->parentID;
      }

      /**
       * @return false if no dir dentry (dir-entry) with this name exists or its type is not dir
       */
      bool getDirDentry(std::string entryName, DirEntry& outEntry)
      {
         bool getRes = getDentry(entryName, outEntry);
         return getRes && DirEntryType_ISDIR(outEntry.getEntryType() );
      }
      
      /**
       *
       * @return false if no dentry with this name exists or its type is not file
       */
      bool getFileDentry(std::string entryName, DirEntry& outEntry)
      {
         bool getRes = getDentry(entryName, outEntry);
         return getRes && DirEntryType_ISFILE(outEntry.getEntryType() );
      }

      bool getEntryInfo(std::string entryName, EntryInfo& outEntryInfo)
      {
         DirEntry entry(entryName);
         std::string parentEntryID = this->getParentEntryID();
         int additionalFlags = 0; // unknown

         bool getRes = getDentry(entryName, entry);
         if (getRes == true)
            entry.getEntryInfo(parentEntryID, additionalFlags, &outEntryInfo);

         return getRes;
      }

      bool getFileEntryInfo(std::string entryName, EntryInfo& outEntryInfo)
      {
         bool getRes = getEntryInfo(entryName, outEntryInfo);
         return getRes && DirEntryType_ISFILE(outEntryInfo.getEntryType() );
      }

      bool getDirEntryInfo(std::string entryName, EntryInfo& outEntryInfo)
      {
         bool getRes = getEntryInfo(entryName, outEntryInfo);
         return getRes && DirEntryType_ISDIR(outEntryInfo.getEntryType() );
      }

      std::string getDirEntryPath()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         std::string dirEntryPath = this->dirEntryPath;

         safeLock.unlock();

         return dirEntryPath;
      }

      // getters & setters
      
      void setParentID(std::string parentID, std::string updatedParentID);
      

   private:

      /**
       * Handle the unlink of a file, for which we need to keep the inode.
       */
      FhgfsOpsErr removeBusyFile(std::string entryName, DirEntry* dentry, bool unlinkEntryame)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // Lock

         FhgfsOpsErr retVal = dentry->removeBusyFile(getDirEntryPathUnlocked(), dentry->getID(),
            entryName, unlinkEntryame);

         safeLock.unlock();

         return retVal;
      }


};


#endif /* DIRENTRYSTORE_H_*/
