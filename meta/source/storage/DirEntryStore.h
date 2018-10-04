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
   ListIncExOutArgs(StringList* outNames, UInt8List* outEntryTypes, StringList* outEntryIDs,
      Int64List* outServerOffsets, int64_t* outNewServerOffset) :
         outNames(outNames), outEntryTypes(outEntryTypes), outEntryIDs(outEntryIDs),
         outServerOffsets(outServerOffsets), outNewServerOffset(outNewServerOffset)
   {
      // see initializer list
   }


   StringList* outNames;           /* required */
   UInt8List* outEntryTypes;       /* optional (can be NULL if caller is not interested; contains
                                      DirEntryType stored as int to ease message serialization) */
   StringList* outEntryIDs;        /* optional (may be NULL if caller is not interested) */
   Int64List* outServerOffsets;    /* optional (may be NULL if caller is not interested) */
   int64_t* outNewServerOffset;    /* optional (may be NULL), equals last value from
                                      outServerOffsets */
};


class DirEntryStore
{
   friend class DirInode;
   friend class MetaStore;

   public:
      DirEntryStore();
      DirEntryStore(const std::string& parentID, bool isBuddyMirrored);

      FhgfsOpsErr makeEntry(DirEntry* entry);

      FhgfsOpsErr linkEntryInDir(const std::string& fromEntryName, const std::string& toEntryName);
      FhgfsOpsErr linkInodeToDir(const std::string& inodePath, const std::string &fileName);

      FhgfsOpsErr removeDir(const std::string& entryName, DirEntry** outDirEntry);
      FhgfsOpsErr unlinkDirEntry(const std::string& entryName, DirEntry* entry,
         unsigned unlinkTypeFlags);

      FhgfsOpsErr renameEntry(const std::string& fromEntryName, const std::string& toEntryName);

      FhgfsOpsErr listIncrementalEx(int64_t serverOffset, unsigned maxOutNames, bool filterDots,
         ListIncExOutArgs& outArgs);
      FhgfsOpsErr listIDFilesIncremental(int64_t serverOffset, uint64_t incrementalOffset,
         unsigned maxOutNames, ListIncExOutArgs& outArgs);

      bool exists(const std::string& entryName);

      FhgfsOpsErr getEntryData(const std::string& entryName, EntryInfo* outInfo,
         FileInodeStoreData* outInodeMetaData);
      FhgfsOpsErr setOwnerNodeID(const std::string& entryName, NumNodeID ownerNode);

      DirEntry* dirEntryCreateFromFile(const std::string& entryName);

      static FhgfsOpsErr mkDentryStoreDir(const std::string& dirID, bool isBuddyMirrored);
      static bool rmDirEntryStoreDir(const std::string& id, bool isBuddyMirrored);

   private:
      std::string parentID; // ID of the directory to which this store belongs
      std::string dirEntryPath; /* path to dirEntry, without the last element (fileName)
                                 * depends on parentID, so changes when parentID is set */

      RWLock rwlock;
      bool isBuddyMirrored;

      FhgfsOpsErr makeEntryUnlocked(DirEntry* entry);
      FhgfsOpsErr linkInodeToDirUnlocked(const std::string& inodePath, const std::string &fileName);


      FhgfsOpsErr removeDirUnlocked(const std::string& entryName, DirEntry** outDirEntry);
      FhgfsOpsErr unlinkDirEntryUnlocked(const std::string& entryName, DirEntry* entry,
         unsigned unlinkTypeFlags);

      bool existsUnlocked(const std::string& entryName);

      const std::string& getDirEntryPathUnlocked() const;


   public:
      // inliners

      /**
       * @return false if no link with this name exists
       */
      bool getDentry(const std::string& entryName, DirEntry& outEntry)
      {
         // note: the difference to getDirDentry/getFileDentry is that this works independent
            // of the link type

         bool exists = false;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

         exists = outEntry.loadFromFileName(getDirEntryPathUnlocked(), entryName);

         safeLock.unlock();

         return exists;
      }

      const std::string& getParentEntryID() const
      {
         return this->parentID;
      }

      /**
       * @return false if no dir dentry (dir-entry) with this name exists or its type is not dir
       */
      bool getDirDentry(const std::string& entryName, DirEntry& outEntry)
      {
         bool getRes = getDentry(entryName, outEntry);
         return getRes && DirEntryType_ISDIR(outEntry.getEntryType() );
      }

      /**
       *
       * @return false if no dentry with this name exists or its type is not file
       */
      bool getFileDentry(const std::string& entryName, DirEntry& outEntry)
      {
         bool getRes = getDentry(entryName, outEntry);
         return getRes && DirEntryType_ISFILE(outEntry.getEntryType() );
      }

      bool getEntryInfo(const std::string& entryName, EntryInfo& outEntryInfo)
      {
         DirEntry entry(entryName);
         std::string parentEntryID = this->getParentEntryID();
         int additionalFlags = 0; // unknown

         bool getRes = getDentry(entryName, entry);
         if (getRes == true)
            entry.getEntryInfo(parentEntryID, additionalFlags, &outEntryInfo);

         return getRes;
      }

      bool getFileEntryInfo(const std::string& entryName, EntryInfo& outEntryInfo)
      {
         bool getRes = getEntryInfo(entryName, outEntryInfo);
         return getRes && DirEntryType_ISFILE(outEntryInfo.getEntryType() );
      }

      bool getDirEntryInfo(const std::string& entryName, EntryInfo& outEntryInfo)
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

      /*
       * Note: No locking here, isBuddyMirrored should only be set on initialization
       */
      bool getIsBuddyMirrored() const
      {
         return this->isBuddyMirrored;
      }

      // getters & setters
      void setParentID(const std::string& parentID, bool parentIsBuddyMirrored);

   private:

      /**
       * Handle the unlink of a file, for which we need to keep the inode.
       */
      FhgfsOpsErr removeBusyFile(const std::string& entryName, DirEntry* dentry,
         unsigned unlinkTypeFlags)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // Lock

         FhgfsOpsErr retVal = dentry->removeBusyFile(getDirEntryPathUnlocked(), dentry->getID(),
            entryName, unlinkTypeFlags);

         safeLock.unlock();

         return retVal;
      }
};


#endif /* DIRENTRYSTORE_H_*/
