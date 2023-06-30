#ifndef DIRENTRY_H_
#define DIRENTRY_H_

#include <common/storage/Metadata.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include <toolkit/StorageTkEx.h>
#include "DentryStoreData.h"
#include "DiskMetaData.h"
#include "MetadataEx.h"
#include "FileInodeStoreData.h"


#define DIRENTRY_LOG_CONTEXT "DirEntry "

#define DirEntry_UNLINK_ID          1
#define DirEntry_UNLINK_FILENAME    2
#define DirEntry_UNLINK_ID_AND_FILENAME (DirEntry_UNLINK_ID | DirEntry_UNLINK_FILENAME)

/*
 * Class for directory entries (aka "dentries", formerly also referred to as "links"), which
 * contains the filename and information about where to find the inode (e.g. for remote dir
 * inodes).
 *
 * Note on locking: In contrast to files/dirs, dentries are not referenced. Every caller/thread
 * gets its own copy to work with, so dentry instances are not even shared. That's why we don't
 * have a mutex here.
 */
class DirEntry
{
   friend class MetaStore;
   friend class DirEntryStore;
   friend class DirInode;
   friend class FileInode;
   friend class GenericDebugMsgEx;
   friend class RecreateDentriesMsgEx;

   public:

      DirEntry(DirEntryType entryType, const std::string& name, const std::string& entryID,
         NumNodeID ownerNodeID) : dentryDiskData(entryID, entryType, ownerNodeID, 0), name(name)
      {
      }

      /**
       * Note: This constructor does not perform initialization, so use it for
       * metadata loading only.
       */
      DirEntry(const std::string& entryName) : name(entryName)
      {
         // this->name = entryName; // set in initializer list
      }

      static DirEntry* createFromFile(const std::string& path, const std::string& entryName);
      static DirEntryType loadEntryTypeFromFile(const std::string& path,
         const std::string& entryName);

   protected:

      bool loadFromID(const std::string& dirEntryPath, const std::string& entryID);

   private:

      DentryStoreData dentryDiskData; // data stored on disk

      FileInodeStoreData inodeData;

      std::string name; // the user-friendly name, note: not set on reading entries anymore

      FhgfsOpsErr storeInitialDirEntryID(const char* logContext, const std::string& idPath);
      static FhgfsOpsErr storeInitialDirEntryName(const char* logContext, const std::string& idPath,
         const std::string& namePath, bool isNonInlinedInode);
      bool storeUpdatedDirEntryBuf(const std::string& idStorePath, char* buf, unsigned bufLen);
      bool storeUpdatedDirEntryBufAsXAttr(const std::string& idStorePath, char* buf,
         unsigned bufLen);
      bool storeUpdatedDirEntryBufAsContents(const std::string& idStorePath, char* buf,
         unsigned bufLen);
      bool storeUpdatedDirEntry(const std::string& dirEntryPath);
      FhgfsOpsErr storeUpdatedInode(const std::string& dirEntryPath);

      static FhgfsOpsErr removeDirEntryName(const char* logContext, const std::string& filePath,
         bool isBuddyMirrored);
      FhgfsOpsErr removeBusyFile(const std::string& dirEntryBasePath, const std::string& entryID,
         const std::string& entryName, unsigned unlinkTypeFlags);

      FileInode* createInodeByID(const std::string& dirEntryPath, EntryInfo* entryInfo);

      bool loadFromFileName(const std::string& dirEntryPath, const std::string& entryName);
      bool loadFromFile(const std::string& path);
      bool loadFromFileXAttr(const std::string& path);
      bool loadFromFileContents(const std::string& path);

      static DirEntryType loadEntryTypeFromFileXAttr(const std::string& path,
         const std::string& entryName);
      static DirEntryType loadEntryTypeFromFileContents(const std::string& path,
         const std::string& entryName);

      FhgfsOpsErr storeInitialDirEntry(const std::string& dirEntryPath);

      static FhgfsOpsErr removeDirEntryFile(const std::string& filePath);
      static FhgfsOpsErr removeDirEntryID(const std::string& dirEntryPath,
         const std::string& entryID, bool isBuddyMirrored);

   public:

      // inliners

      /**
       * Remove file dentries.
       *
       * If the argument is a directory the caller already must have checked if the directory
       * is empty.
       */
      static FhgfsOpsErr removeFileDentry(const std::string& dirEntryPath,
         const std::string& entryID, const std::string& entryName, unsigned unlinkTypeFlags,
         bool isBuddyMirrored)
      {
         const char* logContext = DIRENTRY_LOG_CONTEXT "(remove stored file dentry)";
         FhgfsOpsErr retVal;

         // first we delete entry-by-name and use this retVal as return code
         if (unlinkTypeFlags & DirEntry_UNLINK_FILENAME)
         {
            std::string namePath = dirEntryPath + '/' + entryName;

            retVal = removeDirEntryName(logContext, namePath, isBuddyMirrored);

            if (retVal == FhgfsOpsErr_SUCCESS && (unlinkTypeFlags & DirEntry_UNLINK_ID) )
            {
               // once the dirEntry-by-name was successfully unlinked, unlink dirEntry-by-ID
               removeDirEntryID(dirEntryPath, entryID, isBuddyMirrored); // error code is ignored
            }
            else
            {
               /* We must not try to delete the ID file on  FhgfsOpsErr_NOTEXISTS, as during a race
                * (possible locking issue) the file may have been renamed and so the ID might be
                * still valid.
                */
            }

         }
         else
         if (unlinkTypeFlags & DirEntry_UNLINK_ID)
            retVal = removeDirEntryID(dirEntryPath, entryID, isBuddyMirrored);
         else
         {
            /* It might happen that the code was supposed to unlink an ID only, but if the inode
             * has a link count > 1, even the ID is not supposed to be unlinked. So unlink
             * is a no-op then. */
             retVal = FhgfsOpsErr_SUCCESS;
         }

         return retVal;
      }

      /**
       * Remove directory dentries.
       *
       * If the argument is a directory the caller already must have checked if the directory
       * is empty.
       */
      static FhgfsOpsErr removeDirDentry(const std::string& dirEntryPath,
         const std::string& entryName, bool isBuddyMirrored)
      {
         const char* logContext = DIRENTRY_LOG_CONTEXT "(remove stored directory dentry)";

         std::string namePath = dirEntryPath + '/' + entryName;

         FhgfsOpsErr retVal = removeDirEntryName(logContext, namePath, isBuddyMirrored);

         return retVal;
      }


      // getters & setters


      /**
       * Set a new ownerNodeID, used by fsck or generic debug message.
       */
      bool setOwnerNodeID(const std::string& dirEntryPath, NumNodeID newOwner)
      {
         bool success = true;

         // only dentries without inlined inodes have an ownerNode field, trying to set the owner
         // node on a dentry with an inlined inode is thus impossible.
         if (getIsInodeInlined())
            return false;

         NumNodeID oldOwner = this->getOwnerNodeID();
         this->dentryDiskData.setOwnerNodeID(newOwner);

         if (!storeUpdatedDirEntry(dirEntryPath))
         { // failed to update metadata => restore old value
            this->dentryDiskData.setOwnerNodeID(oldOwner);

            success = false;
         }

         return success;
      }

      void setFileInodeData(FileInodeStoreData& inodeData)
      {
          this->inodeData = inodeData;

          unsigned updatedFlags = getDentryFeatureFlags() |
             (DENTRY_FEATURE_INODE_INLINE | DENTRY_FEATURE_IS_FILEINODE);

          setDentryFeatureFlags(updatedFlags);
      }

      void setBuddyMirrorFeatureFlag()
      {
         addDentryFeatureFlag(DENTRY_FEATURE_BUDDYMIRRORED);
      }

      bool getIsBuddyMirrored()
      {
         return (getDentryFeatureFlags() & DENTRY_FEATURE_BUDDYMIRRORED);
      }

      unsigned getDentryFeatureFlags()
      {
         return this->dentryDiskData.getDentryFeatureFlags();
      }

      void setDentryFeatureFlags(unsigned featureFlags)
      {
         this->dentryDiskData.setDentryFeatureFlags(featureFlags);
      }

      void addDentryFeatureFlag(unsigned featureFlag)
      {
         this->dentryDiskData.addDentryFeatureFlag(featureFlag);
      }

      void removeDentryFeatureFlag(unsigned featureFlag)
      {
         this->dentryDiskData.removeDentryFeatureFlag(featureFlag);
      }

      // getters

      const std::string& getEntryID()
      {
         return this->dentryDiskData.getEntryID();

      }

      const std::string& getID()
      {
         return this->dentryDiskData.getEntryID();
      }

      /**
       * Note: Should not be changed after object init => not synchronized!
       */
      DirEntryType getEntryType()
      {
         return this->dentryDiskData.getDirEntryType();
      }

      const std::string& getName()
      {
         return this->name;
      }

      NumNodeID getOwnerNodeID()
      {
         return this->dentryDiskData.getOwnerNodeID();
      }

      void getEntryInfo(const std::string& parentEntryID, int flags, EntryInfo* outEntryInfo)
      {
         if (getIsInodeInlined() )
            flags |= ENTRYINFO_FEATURE_INLINED;

         if (getIsBuddyMirrored())
            flags |= ENTRYINFO_FEATURE_BUDDYMIRRORED;

         outEntryInfo->set(getOwnerNodeID(), parentEntryID, getID(), name,
            getEntryType(), flags);
      }


      /**
       * Unset the DENTRY_FEATURE_INODE_INLINE flag
       */
      void unsetInodeInlined()
      {
         uint16_t dentryFeatureFlags = this->dentryDiskData.getDentryFeatureFlags();

         dentryFeatureFlags &= ~(DENTRY_FEATURE_INODE_INLINE);

         this->dentryDiskData.setDentryFeatureFlags(dentryFeatureFlags);
      }
      /**
       * Check if the inode is inlined and no flag is set to indicate the same object
       *  (file-as-hard-link) is also in the inode-hash directories.
       */
      bool getIsInodeInlined()
      {
         if (this->dentryDiskData.getDentryFeatureFlags() & DENTRY_FEATURE_INODE_INLINE)
            return true;

         return false;
      }

      void serializeDentry(Serializer& ser)
      {
         DiskMetaData diskMetaData(&this->dentryDiskData, &this->inodeData);
         diskMetaData.serializeDentry(ser);
      }

      void deserializeDentry(Deserializer& des)
      {
         DiskMetaData diskMetaData(&this->dentryDiskData, &this->inodeData);
         diskMetaData.deserializeDentry(des);
      }

   protected:

      FileInodeStoreData* getInodeStoreData(void)
      {
         return &this->inodeData;
      }


};


#endif /* DIRENTRY_H_*/
