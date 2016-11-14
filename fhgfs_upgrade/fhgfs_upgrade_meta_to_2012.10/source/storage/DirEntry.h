#ifndef DIRENTRY_H_
#define DIRENTRY_H_

#include <common/storage/Metadata.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include <toolkit/StorageTkEx.h>
#include <storage/FileInode.h>
#include "MetadataEx.h"
#include "DiskMetaData.h"

#define DIRENTRY_LOG_CONTEXT "DirEntry "

/*
 * Class for directory-entries (dentry). The 'object' usually called 'link' by Sven and Christian.
 * So the typical reference (filename) to an inode.
 *
 * Note on locking: In contrast to files/dirs, entry-links are not referenced. Every caller/thread
 * gets its own copy to work with, so entry-link instances are not even shared. That's why we don't
 * have a mutex here.
 */
class DirEntry : public DiskMetaData
{
   friend class MetaStore;
   friend class DirEntryStore;
   friend class FileInode;

   public:
      /**
       * Note: This constructor does not perform initialization, so use it for
       * metadata loading only.
       */
      DirEntry(std::string entryName)
      {
         this->name = entryName;
         this->dentryFeatureFlags = 0;
      }

      DirEntry(DirEntryType entryType, std::string name, std::string id, uint16_t ownerNodeID);
      
      static DirEntry* createFromFile(std::string path, std::string entryName);
      static DirEntryType loadEntryTypeFromFile(std::string path, std::string entryName);

   private:
      std::string name; // the user-friendly name, note: not set on reading entries anymore

      FhgfsOpsErr storeInitialDirEntryID(const char* logContext, const std::string& path);
      static FhgfsOpsErr storeInitialDirEntryName(const char* logContext, const std::string& idPath,
         const std::string& namePath, bool isDir);
      bool storeUpdatedDirEntryBuf(std::string idStorePath, char* buf, unsigned bufLen);
      bool storeUpdatedDirEntryBufAsXAttr(std::string idStorePath, char* buf, unsigned bufLen);
      bool storeUpdatedDirEntryBufAsContents(std::string idStorePath, char* buf,
         unsigned bufLen);
      bool storeUpdatedDirEntry(std::string dirEntryPath);
      FhgfsOpsErr storeUpdatedInode(std::string dirEntryPath, std::string entryID,
         StatData* newStatData, StripePattern* updatedStripePattern = NULL);

      static FhgfsOpsErr removeDirEntryName(const char* logContext, std::string filePath);
      FhgfsOpsErr removeBusyFile(std::string dirEntryBasePath, std::string entryID,
         std::string entryName, bool unlinkFileName);

      FileInode* createInodeByID(std::string dirEntryPath, std::string entryID);

      bool loadFromFileName(std::string path, std::string entryName);
      bool loadFromID(std::string dirEntryPath, std::string entryID);
      bool loadFromFile(std::string path);
      bool loadFromFileXAttr(std::string path);
      bool loadFromFileContents(std::string path);
      
      static DirEntryType loadEntryTypeFromFileXAttr(const std::string& path,
         const std::string& entryName);
      static DirEntryType loadEntryTypeFromFileContents(const std::string& path,
         const std::string& entryName);

      FhgfsOpsErr storeInitialDirEntry(std::string dirEntryPath);


   public:

      // inliners

      /**
       * Remove the dirEntrID file.
       */
      static FhgfsOpsErr removeDirEntryID(std::string dirEntryPath, std::string entryID)
      {
         const char* logContext = DIRENTRY_LOG_CONTEXT "(remove stored dirEntryID)";

         std::string idPath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) +  entryID;

         FhgfsOpsErr idUnlinkRes = removeDirEntryName(logContext, idPath);

         if (likely(idUnlinkRes == FhgfsOpsErr_SUCCESS) )
            LOG_DEBUG(logContext, 4, "Dir-Entry ID metadata deleted: " + idPath);

         return idUnlinkRes;
      }

      /**
       * Remove file dentries.
       *
       * If the argument is a directory the caller already must have checked if the directory
       * is empty.
       */
      static FhgfsOpsErr removeFileDentry(std::string dirEntryPath, std::string entryID,
         std::string entryName, bool unlinkEntryName)
      {
         const char* logContext = DIRENTRY_LOG_CONTEXT "(remove stored file dentry)";
         FhgfsOpsErr retVal;

         // first we delete entry-by-name and use this retVal as return code
         if (unlinkEntryName)
         {
            std::string namePath = dirEntryPath + '/' + entryName;

            retVal = removeDirEntryName(logContext, namePath);

            if (unlikely(retVal != FhgfsOpsErr_SUCCESS && retVal != FhgfsOpsErr_PATHNOTEXISTS) )
               return retVal;

            // once the dirEntry-by-name was successfully unlinked, unlink dirEntry-by-ID
            removeDirEntryID(dirEntryPath, entryID); // error code is ignored
         }
         else
            retVal = removeDirEntryID(dirEntryPath, entryID);

         return retVal;
      }

      /**
       * Remove directory dentries.
       *
       * If the argument is a directory the caller already must have checked if the directory
       * is empty.
       */
      static FhgfsOpsErr removeDirDentry(std::string dirEntryPath, std::string entryName)
      {
         const char* logContext = DIRENTRY_LOG_CONTEXT "(remove stored directory dentry)";

         std::string namePath = dirEntryPath + '/' + entryName;

         FhgfsOpsErr retVal = removeDirEntryName(logContext, namePath);

         return retVal;
      }


      // getters & setters

      /**
       * Note: Does not update persistent metadata.
       * Note: Unsync'ed; be careful with this!
       */
      void setIDUnpersistent(std::string newID)
      {
         this->entryID = newID;
      }

      bool setOwnerNodeID(std::string dirEntryPath, uint16_t newOwner)
      {
         bool success = true;
         
         uint16_t oldOwner = this->ownerNodeID;
         this->ownerNodeID = newOwner;
         
         if(!storeUpdatedDirEntry(dirEntryPath) )
         { // failed to update metadata => restore old value
            this->ownerNodeID = oldOwner;
            
            success = false;
         }

         return success;
      }

      void setDentryInodeMeta(DentryInodeMeta& inodeMetaData)
      {
          this->inodeData = inodeMetaData;

          unsigned updatedFlags = this->dentryFeatureFlags |=
             (DISKMETADATA_FEATURE_INODE_INLINE | DISKMETADATA_FEATURE_IS_FILEINODE);

          setFeatureFlags(updatedFlags);
      }

      void setFeatureFlags(unsigned featureFlags)
      {
         this->dentryFeatureFlags = featureFlags;
      }


      // getters


      std::string getID()
      {
         return entryID;
      }

      /**
       * Note: Should not be changed after object init => not synchronized!
       */
      DirEntryType getEntryType()
      {
         return entryType;
      }

      std::string getName()
      {
         return this->name;
      }

      uint16_t getOwnerNodeID()
      {
         return ownerNodeID;
      }

      DentryInodeMeta* getDentryInodeMeta()
      {
         // inodeData feature flags might not be up-to-date
         this->inodeData.setDentryFeatureFlags(this->dentryFeatureFlags);

         return &this->inodeData;
      }

      void getEntryInfo(std::string& parentEntryID, int flags, EntryInfo* outEntryInfo)
      {
         if (this->dentryFeatureFlags & DISKMETADATA_FEATURE_INODE_INLINE)
            flags |= ENTRYINFO_FLAG_INLINED;

         outEntryInfo->update(ownerNodeID, parentEntryID, entryID, name, entryType, flags);
      }

      unsigned getFeatureFlags(void)
      {
         return this->dentryFeatureFlags;
      }

      /**
       * Check if the inode is inlined and no flag is set to indicate the same object
       *  (file-as-hard-link) is also in the inode-hash directories.
       */
      bool isInodeInlined(void)
      {
         if (this->dentryFeatureFlags & DISKMETADATA_FEATURE_INODE_INLINE)
            return true;

         return false;
      }


      bool mapStringToNumID(StringUnsignedMap* idMap, std::string inStrID, uint16_t& outNumID)
      {
         StringUnsignedMapIter mapIter = idMap->find(inStrID);
         if (unlikely(mapIter == idMap->end() ) )
            return false; // not in map

         outNumID = mapIter->second;
         return true;

      }

      /**
       * Map strID to numID
       */
      bool mapOwnerNodeStrID(StringUnsignedMap* metaIdMap)
      {
         if (!mapStringToNumID(metaIdMap, this->ownerNodeIDStr, this->ownerNodeID) )
            return false;

         return true;
      }

};


#endif /* DIRENTRY_H_*/
