#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/Storagedata.h>
#include <common/toolkit/FsckTk.h>
#include <net/msghelpers/MsgHelperStat.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <program/Program.h>
#include "MetaStore.h"

#include <attr/xattr.h>
#include <dirent.h>
#include <sys/types.h>

/**
 * Reference the given directory.
 *
 * @param dirID      the ID of the directory to reference
 * @param forceLoad  not all operations require the directory to be loaded from disk (i.e. stat()
 *                   of a sub-entry) and the DirInode is only used for directory locking. Other
 *                   operations need the directory to locked and those shall set forceLoad = true.
 *                   Should be set to true if we are going to write to the DirInode anyway or
 *                   if DirInode information are required.
 * @return           cannot be NULL if forceLoad=false, even if the directory with the given id
 *                   does not exist. But it can be NULL of forceLoad=true, as we only then
 *                   check if the directory exists on disk at all.
 */
DirInode* MetaStore::referenceDir(std::string dirID, bool forceLoad)
{
   DirInode* dirInode;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   dirInode = referenceDirUnlocked(dirID, forceLoad);

   safeLock.unlock(); // U N L O C K

   return dirInode;
}

DirInode* MetaStore::referenceDirUnlocked(std::string dirID, bool forceLoad)
{
   FhgfsOpsErr checkDirRes = this->checkRmDirMap(dirID);
   if (unlikely(checkDirRes == FhgfsOpsErr_PATHNOTEXISTS) )
      return NULL;
   else
      return dirStore.referenceDirInode(dirID, forceLoad);
}

/**
 * Reference the directory inode and lock it. We don't advise to load the dirInode from disk here,
 * so with minor exceptions (e.g. out of memory) we will always get the dirInode.
 * Also aquire a read-lock for the directory.
 */
DirInode* MetaStore::referenceAndReadLockDirUnlocked(std::string dirID)
{
   DirInode *dirInode = referenceDirUnlocked(dirID, false);
   if (likely(dirInode))
      dirInode->rwlock.readLock();

   return dirInode;
}

/**
 * Similar to referenceAndReadLockDirUnlocked(), but write-lock here.
 */
DirInode* MetaStore::referenceAndWriteLockDirUnlocked(std::string dirID)
{
   DirInode *dirInode = referenceDirUnlocked(dirID, false);
   if (likely(dirInode))
      dirInode->rwlock.writeLock();

   return dirInode;
}

void MetaStore::releaseDir(std::string dirID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   releaseDirUnlocked(dirID);

   safeLock.unlock(); // U N L O C K
}

void MetaStore::releaseDirUnlocked(std::string dirID)
{
   dirStore.releaseDir(dirID);
}

/**
 * Unlock a release a directory reference.
 */
void MetaStore::unlockAndReleaseDirUnlocked(DirInode* dir)
{
   dir->rwlock.unlock();

   releaseDirUnlocked(dir->getID() );
}

/**
 * Only unlock a directory here.
 */
void MetaStore::unlockDirUnlocked(DirInode* dir)
{
   dir->rwlock.unlock();
}


/**
 * Reference a file. It is unknown if this file is already referenced in memory or needs to be
 * loaded. Therefore a complete entryInfo is required.
 */
FileInode* MetaStore::referenceFile(EntryInfo* entryInfo)
{
   FileInode* retVal;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   retVal = referenceFileUnlocked(entryInfo);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * See referenceFileInode() for details. We already have the lock here.
 */
FileInode* MetaStore::referenceFileUnlocked(EntryInfo* entryInfo)
{
   FileInode *inode = this->fileStore.referenceFileInode(entryInfo, false);
   if (!inode)
   {
      // not in global map, now per directory and also try to load from disk

      // FIXME Bernd: really need dir-lock here?
      DirInode* subDir = referenceAndReadLockDirUnlocked(entryInfo->getParentEntryID() );
      if (likely(subDir) )
      {
         inode = subDir->fileStore.referenceFileInode(entryInfo, true);

         if (!inode)
            unlockAndReleaseDirUnlocked(subDir);
         else
         {
            // we are not going to release the directory here, as we are using its FileStore
            inode->incParentRef();
            unlockDirUnlocked(subDir);
         }
      }
   }

   return inode;
}

/**
 * Reference a file known to be already referenced. So disk-access is not required and we don't
 * need the complete EntryInfo, but parentEntryID and entryID are sufficient.
 * Another name for this function also could be "referenceReferencedFiles".
 */
FileInode* MetaStore::referenceLoadedFile(std::string parentEntryID, std::string entryID)
{
   FileInode* retVal;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   retVal = referenceLoadedFileUnlocked(parentEntryID, entryID);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * See referenceLoadedFile() for details. We already have the lock here.
 */
FileInode* MetaStore::referenceLoadedFileUnlocked(std::string parentEntryID, std::string entryID)
{
   FileInode *inode = this->fileStore.referenceLoadedFile(entryID);
   if (!inode)
   {
      // not in global map, now per directory and also try to load from disk
      DirInode* subDir = referenceAndReadLockDirUnlocked(parentEntryID);
      if (likely(subDir) )
      {
         inode = subDir->fileStore.referenceLoadedFile(entryID);

         if (!inode)
            unlockAndReleaseDirUnlocked(subDir);
         else
         {
            // we are not going to release the directory here, as we are using its FileStore
            inode->incParentRef();
            unlockDirUnlocked(subDir);
         }
      }
   }

   return inode;
}

bool MetaStore::releaseFile(std::string parentEntryID, FileInode* inode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   bool releaseRes = releaseFileUnlocked(parentEntryID, inode);

   safeLock.unlock(); // U N L O C K

   return releaseRes;
}

/**
 * Release a file inode.
 *
 * Note: If the inode belongs to the per-directory file store also this directory will be
 *       released.
 */
bool MetaStore::releaseFileUnlocked(std::string parentEntryID, FileInode* inode)
{
   bool releaseRes = fileStore.releaseFileInode(inode);
   if (!releaseRes)
   { // Not in global map, now per directory and also try to load from disk.
      DirInode* subDir = referenceAndReadLockDirUnlocked(parentEntryID );
      if (likely(subDir) )
      {
         inode->decParentRef();
         releaseRes = subDir->fileStore.releaseFileInode(inode);

         unlockAndReleaseDirUnlocked(subDir); // this is the current lock and reference

         // when we referenced the inode we did not release the directory yet, so do that here
         releaseDirUnlocked(parentEntryID);
      }
   }

   return releaseRes;
}

/*
 * Can be used to reference an inode, if it is unknown whether it is a file or directory. While
 * one of the out.... pointers will hold a reference to the corresponidng inode (which must be
 * released), the other pointer will be set to NULL.
 *
 * Note: This funtion is mainly used by fsck and the fullRefresher component.
 * Note: dirInodes will always be loaded from disk with this function
 * Note: This will NOT work with inlined file inodes, as we only pass the ID
 * Note: This is not very performant, but OK for now. Can definitely be optimized.
 *
 * @param entryID
 * @param outDirInode
 * @param outFilenode
 *
 * @return the DirEntryType of the inode
 */
DirEntryType MetaStore::referenceInode(std::string entryID, FileInode** outFileInode,
   DirInode** outDirInode)
{
   *outFileInode = NULL;
   *outDirInode  = NULL;

   DirInode* dirInode = NULL;
   FileInode* fileInode = NULL;

   // trying dir first, because it is assumed that there are more dir inodes than file inodes (file
   // inodes should mostly be inlined in dentry)
   dirInode = this->referenceDir(entryID, true);

   if (dirInode)
   {
      *outDirInode = dirInode;
      return DirEntryType_DIRECTORY;
   }

   // if opening as dir failed try it as file
   // we do not set full entryInfo (we do not have most of the info), but only entryID. That's why
   // it does not work with inlined inodes
   EntryInfo entryInfo(0, "", entryID, "", DirEntryType_REGULARFILE,0);

   fileInode = this->referenceFile(&entryInfo);

   if (fileInode)
   {
      *outFileInode = fileInode;
      return DirEntryType_REGULARFILE;
   }

   // neither worked as dir nor as file
   return DirEntryType_INVALID;
}


FhgfsOpsErr MetaStore::isFileUnlinkable(DirInode* subDir, EntryInfo* entryInfo,
   FileInode** outInode)
{
   FhgfsOpsErr isUnlinkable = this->fileStore.isUnlinkable(entryInfo, false, outInode);
   if (isUnlinkable == FhgfsOpsErr_SUCCESS)
      isUnlinkable = subDir->fileStore.isUnlinkable(entryInfo, false, outInode);

   return isUnlinkable;
}


/**
 * Move a fileInode reference from subDir->fileStore to (MetaStore) this->fileStore
 *
 * @param subDir can be NULL, the dir then must not be locked at all. If subDir is not NULL,
 *               it must be write-locked already.
 * @return true if an existing reference was moved
 *
 * Note: MetaStore needs to be write locked!
 */
bool MetaStore::moveReferenceToMetaFileStoreUnlocked(std::string parentEntryID, std::string entryID)
{
   bool retVal = false;
   const char* logContext = "MetaStore (Move reference from per-Dir fileStore to global Store)";

   DirInode* subDir = referenceAndWriteLockDirUnlocked(parentEntryID);
   if (unlikely(!subDir) )
       return false;

   FileInodeReferencer* inodeRefer = subDir->fileStore.getReferencerAndDeleteFromMap(entryID);
   if (inodeRefer)
   {
      // The inode is referenced in the per-directory fileStore. Move it to the global store.

      FileInode* inode = inodeRefer->reference();
      ssize_t numParentRefs = inode->getNumParentRefs();

      retVal = this->fileStore.insertReferencer(entryID, inodeRefer);
      if (unlikely(retVal == false) )
      {
         std::string msg = "Bug: Failed to move to MetaStore fileStore - already exists in map";
         LogContext(logContext).logErr(msg);

         // FIXME Bernd: What is the best way to handle this?
      }

      LOG_DEBUG(logContext, Log_SPAM, std::string("Releasing dir references entryID: ") +
         entryID + " dirID: " + subDir->getID() );
      while (numParentRefs > 0)
      {
         releaseDirUnlocked(parentEntryID); /* inode references also always keep a dir reference, so
                                             * we need to release the dir as well */
         numParentRefs--;
         inode->decParentRef();
      }

      inodeRefer->release();
   }

   unlockAndReleaseDirUnlocked(subDir);

   return retVal;
}

/**
 * @param accessFlags OPENFILE_ACCESS_... flags
 */
FhgfsOpsErr MetaStore::openFile(EntryInfo* entryInfo, unsigned accessFlags, FileInode** outInode)
{
   FhgfsOpsErr retVal;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   /* check the MetaStore fileStore map here, but most likely the file will not be in this map,
    * but is either in the per-directory-map or has to be loaded from disk */
   if (this->fileStore.isInStore(entryInfo->getEntryID() ) )
   {
      retVal = this->fileStore.openFile(entryInfo, accessFlags, outInode, false);
   }
   else
   {
      // not in global map, now per directory and also try to load from disk
      DirInode* subDir = referenceAndReadLockDirUnlocked(entryInfo->getParentEntryID() );
      if (likely(subDir) )
      {
         retVal = subDir->fileStore.openFile(entryInfo, accessFlags, outInode, true);

         if (*outInode)
         {
            (*outInode)->incParentRef();
            unlockDirUnlocked(subDir); /* Do not release dir here, we are returning the inode
                                        * referenced in subDirs fileStore! */
         }
         else
            unlockAndReleaseDirUnlocked(subDir);
      }
      else
         retVal = FhgfsOpsErr_INTERNAL;
   }

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * @param accessFlags OPENFILE_ACCESS_... flags
 * @param outNumHardlinks for quick on-close unlink check
 * @param outNumRef also for on-close unlink check
 */
void MetaStore::closeFile(EntryInfo* entryInfo, FileInode* inode, unsigned accessFlags,
   unsigned* outNumHardlinks, unsigned* outNumRefs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   // now release (and possible free (delete) the inode if not further referenced)

   // first try global metaStore map
   bool closeRes = this->fileStore.closeFile(entryInfo, inode, accessFlags,
      outNumHardlinks, outNumRefs);
   if (!closeRes)
   {
      // not in global /store/map, now per directory
      DirInode* subDir = referenceAndReadLockDirUnlocked(entryInfo->getParentEntryID() );
      if (likely(subDir) )
      {
         inode->decParentRef(); /* Already decrease it here, as the inode might get destroyed
                                 * in closeFile(). The counter is very important if there is
                                 * another open reference on this file and if the file is unlinked
                                 * while being open */

         closeRes = subDir->fileStore.closeFile(entryInfo, inode, accessFlags,
            outNumHardlinks, outNumRefs);

         unlockAndReleaseDirUnlocked(subDir);

         // we kept another dir reference in openFile(), so release it here
         releaseDirUnlocked(entryInfo->getParentEntryID() );
      }
   }

   safeLock.unlock(); // U N L O C K
}

FhgfsOpsErr MetaStore::stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData)
{
   FhgfsOpsErr statRes = FhgfsOpsErr_PATHNOTEXISTS;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   if (entryInfo->getEntryType() == DirEntryType_DIRECTORY)
   { // entry is a dir
      statRes = dirStore.stat(entryInfo->getEntryID(), outStatData);
   }
   else
   { // entry is any type, but a directory, e.g. a regular file

      // first check if the inode is referenced in the global store
      statRes = fileStore.stat(entryInfo, false, outStatData); // do not try to load from disk!

      if (statRes == FhgfsOpsErr_PATHNOTEXISTS) // not in global store, now per directory
      {
         // not in global /store/map, now per directory
         DirInode* subDir = referenceAndReadLockDirUnlocked(entryInfo->getParentEntryID() );
         if (likely(subDir))
         {
            statRes = subDir->fileStore.stat(entryInfo, loadFromDisk, outStatData);

            unlockAndReleaseDirUnlocked(subDir);
         }
      }
   }

   safeLock.unlock(); // U N L O C K

   return statRes;
}

FhgfsOpsErr MetaStore::setAttr(EntryInfo* entryInfo, int validAttribs, SettableFileAttribs* attribs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr setAttrRes = setAttrUnlocked(entryInfo, validAttribs, attribs);

   safeLock.unlock(); // U N L O C K

   return setAttrRes;
}

/**
 * @param validAttribs SETATTR_CHANGE_...-Flags, but maybe NULL if we only want to update
 *   attribChangeTimeSecs
 * @param attribs  new attribs, but maybe NULL if validAttribs == 0
 */
FhgfsOpsErr MetaStore::setAttrUnlocked(EntryInfo* entryInfo, int validAttribs,
   SettableFileAttribs* attribs)
{
   FhgfsOpsErr setAttrRes = FhgfsOpsErr_PATHNOTEXISTS;

   std::string localNodeID = Program::getApp()->getLocalNode()->getID();

   if (DirEntryType_ISDIR(entryInfo->getEntryType()) )
      setAttrRes = dirStore.setAttr(entryInfo->getEntryID(), validAttribs, attribs);
   else
   {
      if (this->fileStore.isInStore(entryInfo->getEntryID() ) )
         setAttrRes = this->fileStore.setAttr(entryInfo, validAttribs, attribs);
      else
      {
         // not in global /store/map, now per directory
         DirInode* subDir = referenceAndReadLockDirUnlocked(entryInfo->getParentEntryID() );
         if (likely(subDir) )
         {
            setAttrRes = subDir->fileStore.setAttr(entryInfo, validAttribs, attribs);

            unlockAndReleaseDirUnlocked(subDir);
         }
      }
   }

   return setAttrRes;
}

/**
 * Create a directory-entry with an inlined inode
 *
 * @param preferredTargets  will be ignored if inStripePattern is defined
 * @param outEntryInfo      will not be set if NULL (caller not interested)
 * @param outInodeData      not not be set if NULL (caller not interested)
 * @param inStripePattern   NULL by default, caller *may* provide it and it will be used (and
 *                          destructed here)
 */
FhgfsOpsErr MetaStore::mkMetaFile(DirInode* dir, MkFileDetails* mkDetails,
   UInt16List* preferredTargets, StripePattern* inStripePattern, EntryInfo* outEntryInfo,
   DentryInodeMeta* outInodeData)
{
   SafeRWLock metaLock(&this->rwlock, SafeRWLock_READ);
   SafeRWLock dirLock(&dir->rwlock, SafeRWLock_WRITE); // L O C K ( dir )

   FhgfsOpsErr retVal = mkMetaFileUnlocked(dir, mkDetails, preferredTargets, inStripePattern,
      outEntryInfo, outInodeData);

   dirLock.unlock();
   metaLock.unlock();

   return retVal;
}

/**
 * @param dir         Already needs to be locked by the caller.
 * @param cloneInode  Take values from this inode to create the new file. Object will be destroyed
 *                    here.
 */
FhgfsOpsErr MetaStore::mkMetaFileUnlocked(DirInode* dir, std::string entryName,
   DirEntryType entryType, FileInode* inode)
{
   App* app = Program::getApp();
   Node* localNode = app->getLocalNode();

   StripePattern* stripePattern = inode->getStripePattern();
   StatData* statData = inode->getNotUpdatedStatDataUseCarefully();
   std::string entryID = inode->getUpdatedID();

   unsigned chunkHash = StorageTk::getChunkHash(entryID,
      CONFIG_CHUNK_LEVEL1_SUBDIR_NUM, CONFIG_CHUNK_LEVEL2_SUBDIR_NUM);

   // clones stripePattern
   DentryInodeMeta inodeMetaData(stripePattern, entryID, statData, chunkHash);

   inodeMetaData.setInodeFeatureFlags(inode->getFeatureFlags() );

   uint16_t ownerNodeID = localNode->getNumID();
   DirEntry newDentry (entryType, entryName, entryID, ownerNodeID);

   newDentry.setDentryInodeMeta(inodeMetaData);
   inodeMetaData.setPattern(NULL); // pattern now in newDentry, make sure it won't be deleted on
                                   // object destruction

   // create a dir-entry with inlined inodes
   FhgfsOpsErr makeRes = dir->makeDirEntryUnlocked(&newDentry, false);

   delete inode;

   return makeRes;
}

/**
 * note: @param dir already needs to be locked by the caller
 */
FhgfsOpsErr MetaStore::mkMetaFileUnlocked(DirInode* dir, MkFileDetails* mkDetails,
   UInt16List* preferredTargets, StripePattern* inStripePattern, EntryInfo* outEntryInfo,
   DentryInodeMeta* outInodeData)
{
   const char* logContext = "MetaStore::mkMetaFile (create file)";

   App* app = Program::getApp();
   Node* localNode = app->getLocalNode();

   // create new stripe pattern

   StripePattern* stripePattern;
   if (!inStripePattern)
      stripePattern = dir->createFileStripePatternUnlocked(preferredTargets);
   else
      stripePattern = inStripePattern;

   // check availability of stripe targets
   if(unlikely(!stripePattern ||  stripePattern->getStripeTargetIDs()->empty() ) )
   {
      LogContext(logContext).logErr(
         "Unable to create stripe pattern. No storage targets available?");

      SAFE_DELETE(stripePattern);
      return FhgfsOpsErr_INTERNAL;
   }

   std::string newEntryID = StorageTk::generateFileID(localNode->getNumID() );
   uint16_t ownerNodeID = localNode->getNumID();

   DirEntryType entryType = MetadataTk::posixFileTypeToDirEntryType(mkDetails->mode);
   StatData statData(mkDetails->mode, mkDetails->userID, mkDetails->groupID);

   unsigned chunkHash = StorageTk::getChunkHash(newEntryID,
      CONFIG_CHUNK_LEVEL1_SUBDIR_NUM, CONFIG_CHUNK_LEVEL2_SUBDIR_NUM);

   // clones stripePattern
   DentryInodeMeta inodeMetaData(stripePattern, newEntryID, &statData, chunkHash);

   DirEntry* newDentry = new DirEntry(entryType, mkDetails->newName, newEntryID, ownerNodeID);

   newDentry->setDentryInodeMeta(inodeMetaData);
   inodeMetaData.setPattern(NULL); // pattern now in newDentry, make sure it won't be deleted on
                                   // object destruction

   // create a dir-entry with inlined inodes
   FhgfsOpsErr makeRes = dir->makeDirEntryUnlocked(newDentry, false);
   if(makeRes != FhgfsOpsErr_SUCCESS)
   {
      /* addding the dir-entry failed, but we don't need to do delete anything, as an incorrectly
       * written dir-entry (so writing initial data failed) was already deleted by makeDirEntry()
       * and as we don't have a separate inode, we also do not need to care about that */

      delete(stripePattern);
   }
   else
   { // new entry successfully created
      std::string parentEntryID = dir->getID();

      int dentryFeatureFlags = newDentry->getFeatureFlags();

      if (outInodeData)
      {
         outInodeData->setInodeStatData(&statData);
         outInodeData->setPattern(stripePattern);
         outInodeData->setID(newEntryID);
         outInodeData->setDentryFeatureFlags(dentryFeatureFlags);
         outInodeData->setChunkHash(chunkHash);
      }
      else // caller not interested in those data
         delete stripePattern;

      unsigned flags = 0;
      if (dentryFeatureFlags & DISKMETADATA_FEATURE_INODE_INLINE)
         flags = ENTRYINFO_FLAG_INLINED;

      if (outEntryInfo)
         outEntryInfo->update(ownerNodeID, parentEntryID, newEntryID, mkDetails->newName,
            entryType, flags);
   }

   delete newDentry;

   return makeRes;
}

FhgfsOpsErr MetaStore::makeFileInode(FileInode* inode, bool keepInode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr mkRes = fileStore.makeFileInode(inode, keepInode);

   safeLock.unlock(); // U N L O C K

   return mkRes;
}


FhgfsOpsErr MetaStore::makeDirInode(DirInode* inode, bool keepInode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr mkRes = dirStore.makeDirInode(inode, keepInode);

   safeLock.unlock(); // U N L O C K

   return mkRes;
}

FhgfsOpsErr MetaStore::removeDirInode(std::string entryID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

#if 0
   RmDirMapValues* rmDirValues = new RmDirMapValues;

   rmDirValues->mutex.lock(); // this must be done before adding it to the rmDirMap!

   std::pair<DirectoryMapIter, bool> pairRes =
      this->rmDirMap.insert(RmDirMapVal(entryID, rmDirValues) );

   if (pairRes.second == false)
   {
      // FIXME Bernd: Already in map
   }

#endif

   FhgfsOpsErr rmRes = dirStore.removeDirInode(entryID);

   safeLock.unlock(); // U N L O C K

   return rmRes;
}

/**
 * @param subDir may be NULL and then needs to be referenced
 */
FhgfsOpsErr MetaStore::unlinkInodeUnlocked(EntryInfo* entryInfo, DirInode* subDir,
   FileInode** outInode)
{
   FhgfsOpsErr unlinkRes;

   if (this->fileStore.isInStore(entryInfo->getEntryID() ) )
      unlinkRes = fileStore.unlinkFileInode(entryInfo, outInode);
   else
   {
      if (!subDir)
      {
         // not in global /store/map, now per directory
         subDir = referenceAndReadLockDirUnlocked(entryInfo->getParentEntryID() );
         if (likely(subDir) )
         {
            unlinkRes = subDir->fileStore.unlinkFileInode(entryInfo, outInode);

            unlockAndReleaseDirUnlocked(subDir); /* we can release the DirInode here, as the
                                                  * FileInode is not supposed to be in the
                                                  * DirInodes FileStore anymore */
         }
         else
            unlinkRes = FhgfsOpsErr_PATHNOTEXISTS;
      }
      else // subDir already referenced and locked
         unlinkRes = subDir->fileStore.unlinkFileInode(entryInfo, outInode);
   }

   return unlinkRes;
}


/**
 * @param outFile will be set to the unlinked file and the object must then be deleted by the caller
 * (can be NULL if the caller is not interested in the file)
 */
FhgfsOpsErr MetaStore::unlinkInode(EntryInfo* entryInfo, FileInode** outInode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr unlinkRes = this->unlinkInodeUnlocked(entryInfo, NULL, outInode);

   safeLock.unlock(); // U N L O C K

   return unlinkRes;
}

/**
 * need the following locks:
 *   this->rwlock: SafeRWLock_WRITE
 *   subDir: reference
 *   subdir->rwlock: SafeRWLock_WRITE
 *
 *   note: caller needs to delete storage chunk files. E.g. via MsgHelperUnlink::unlinkLocalFile()
 */
FhgfsOpsErr MetaStore::unlinkFileUnlocked(DirInode* subdir, std::string fileName,
   FileInode** outInode, EntryInfo* outEntryInfo, bool& outWasInlined)
{
   FhgfsOpsErr retVal;

   DirEntry* dirEntry = subdir->dirEntryCreateFromFileUnlocked(fileName);
   if (!dirEntry)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // dirEntry exists time to make sure we have loaded subdir
   bool loadRes = subdir->loadIfNotLoadedUnlocked();
   if (unlikely(!loadRes) )
      return FhgfsOpsErr_INTERNAL; // if dirEntry exists, subDir also has to exist!

   // set the outEntryInfo
   int additionalFlags = 0;
   std::string parentEntryID = subdir->getID();
   dirEntry->getEntryInfo(parentEntryID, additionalFlags, outEntryInfo);

   if (dirEntry->isInodeInlined() )
   {  // inode is inlined into the dir-entry
      retVal = unlinkDirEntryWithInlinedInodeUnlocked(fileName, subdir, dirEntry, outInode,
         true);

      outWasInlined = true;
   }
   else
   {  // inode and dir-entry are separated fileStore
      retVal = unlinkDentryAndInodeUnlocked(fileName, subdir, dirEntry, true, outInode);

      outWasInlined = false;
   }

   delete dirEntry; // we explicitly checked above if dirEntry != NULL, so no SAFE required

   return retVal;
}

/**
 * Unlinks the entire file, so dir-entry and inode.
 *
 * @param fileName friendly name
 * @param outInode will be set to the unlinked (owned) file and the object and
 * storage server fileStore must then be deleted by the caller; even if success is returned,
 * this might be NULL (e.g. because the file is in use and was added to the disposal directory);
 * can be NULL if the caller is not interested in the file
 * @return normal fhgfs error code, normally succeeds even if a file was open; special case is
 * when this is called to unlink a file with the disposalDir dirID, then an open file will
 * result in a inuse-error (required for online_cfg mode=dispose)
 *
 * note: caller needs to delete storage chunk files. E.g. via MsgHelperUnlink::unlinkLocalFile()
 */
FhgfsOpsErr MetaStore::unlinkFile(std::string dirID, std::string fileName, FileInode** outInode)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;
   std::string fileID;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DirInode* subdir = referenceAndWriteLockDirUnlocked(dirID);
   if(!subdir)
   {
      safeLock.unlock();
      return retVal;
   }

   EntryInfo entryInfo;
   bool wasInlined;
   retVal = unlinkFileUnlocked(subdir, fileName, outInode, &entryInfo, wasInlined);

   unlockAndReleaseDirUnlocked(subdir); // we can release the inode here, as it is not supposed
                                        // to be in the DirInodes FileStore anymore.

   /* Give up the read-lock here, unlinkInodeLater() will aquire a write lock. We already did most
    * of the work, just possible back linking of the inode to the disposal dir is missing.
    * As our important work is done, we can also risk to give up the read lock. */
   safeLock.unlock(); // U N L O C K

   if (retVal == FhgfsOpsErr_INUSE)
   {
         unlinkInodeLater(&entryInfo, wasInlined);

         retVal = FhgfsOpsErr_SUCCESS;
   }

   return retVal;
}

/**
 * Unlink a dirEntry with an inlined inode
 */
FhgfsOpsErr MetaStore::unlinkDirEntryWithInlinedInodeUnlocked(std::string entryName,
   DirInode* subDir, DirEntry* dirEntry, FileInode** outInode, bool unlinkEntryName)
{
   FhgfsOpsErr retVal;

   std::string parentEntryID = subDir->getID();

   // when we are here, we no the inode is inlined into the dirEntry
   int flags = ENTRYINFO_FLAG_INLINED;

   EntryInfo entryInfo;
   dirEntry->getEntryInfo(parentEntryID, flags, &entryInfo);

   FhgfsOpsErr isUnlinkable = isFileUnlinkable(subDir, &entryInfo, outInode);

   // note: FhgfsOpsErr_PATHNOTEXISTS cannot happen with isUnlinkable(id, false)


   if (isUnlinkable == FhgfsOpsErr_INUSE)
   {
      // for some reasons we cannot unlink the inode, probably the file is opened. De-inline it.

      bool unlinkRes = subDir->unlinkBusyFileUnlocked(entryName, dirEntry, unlinkEntryName);
      if (unlinkRes)
         retVal = FhgfsOpsErr_INTERNAL;
      else
         retVal = isUnlinkable;

      /* We are done here, but the file still might be referenced in the per-dir file store.
       * However, we cannot move it, as we do not have a MetaStore write-lock, but only a
       * read-lock. Therefore that has to be done later on, once we have given up the read-lock. */
   }
   else
   if (isUnlinkable == FhgfsOpsErr_SUCCESS)
   {
      // dir-entry and inode are inlined. The file is also not opened anymore, so delete it.

      if (outInode)
         *outInode = FileInode::createFromEntryInfo(&entryInfo);

      retVal = subDir->unlinkDirEntryUnlocked(entryName, dirEntry, unlinkEntryName, NULL);
   }
   else
      retVal = isUnlinkable;

   return retVal;
}

/**
 * Unlink seperated dirEntry and Inode
 */
FhgfsOpsErr MetaStore::unlinkDentryAndInodeUnlocked(std::string fileName,
   DirInode* subdir, DirEntry* dirEntry, bool unlinkEntryName, FileInode** outInode)
{
   // unlink dirEntry first
   FhgfsOpsErr retVal = subdir->unlinkDirEntryUnlocked(fileName, dirEntry, unlinkEntryName, NULL);

   if(retVal == FhgfsOpsErr_SUCCESS)
   { // directory-entry was removed => unlink inode

      // if we are here we know the dir-entry does not inline the inode

      int addionalEntryInfoFlags = 0;
      std::string parentEntryID = subdir->getID();
      EntryInfo entryInfo;

      dirEntry->getEntryInfo(parentEntryID, addionalEntryInfoFlags, &entryInfo);

      FhgfsOpsErr unlinkFileRes = this->unlinkInodeUnlocked(&entryInfo, subdir, outInode);
      if(unlinkFileRes != FhgfsOpsErr_SUCCESS && unlinkFileRes == FhgfsOpsErr_INUSE)
         retVal = FhgfsOpsErr_INUSE;
   }

   return retVal;
}

/**
 * Adds the entry to the disposal directory for later (asynchronous) disposal.
 */
void MetaStore::unlinkInodeLater(EntryInfo* entryInfo, bool wasInlined)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   unlinkInodeLaterUnlocked(entryInfo, wasInlined);

   safeLock.unlock(); // U N L O C K
}

/**
 * Adds the inode (with a new dirEntry) to the disposal directory for later
 * (on-close or asynchronous) disposal.
 */
void MetaStore::unlinkInodeLaterUnlocked(EntryInfo* entryInfo, bool wasInlined)
{
   // Note: We must not try to unlink the inode here immediately, because then the local versions
   //       of the data-object (on the storage nodes) would never be deleted.

   App* app = Program::getApp();

   std::string parentEntryID = entryInfo->getParentEntryID();
   std::string entryID = entryInfo->getEntryID();

   DirInode* disposalDir = app->getDisposalDir();
   std::string disposalID = disposalDir->getID();

   if (parentEntryID == disposalID)
      return; // nothing for us to do, the inode is already in the disposal dir

   /* This requires a MetaStore write lock, therefore only can be done here and not in common
    * unlink code, as the common unlink code only has a MetaStore read-lock. */
   if (!this->fileStore.isInStore(entryID) )
      moveReferenceToMetaFileStoreUnlocked(parentEntryID, entryID); // FIXME Bernd: check return code?

   entryInfo->setParentEntryID(disposalID); // update the dirID to disposalDir

   // set inode nlink count to 0.
   // we assume the inode is typically already referenced by someone (otherwise we wouldn't need to
   // unlink later) and this allows for faster checking on-close than the disposal dir check.

   // do not load the inode from disk first.
   FileInode* inode = this->fileStore.referenceFileInode(entryInfo, false);

   /* FIXME Bernd: If we cannot reference the inode from memory, we raced with close().
    *              This is possible as we gave up all locks in the calling unlink code.
    *              It means the inode shall be deleted entirely now.
    *              TODO: Unset entryInfo inlined flag. Load the inode from disk. And return with
    *                    the inode object, which we can pass to the underlying calling code.
    *                    Difficult to test, so write a unit test.
    */

   int linkCount = -1; // undefined

   if(likely(inode) )
   {
      // we need to decrease the link count if the file was not inlined

      linkCount = inode->getNumHardlinks();
      if (likely (linkCount > 0) )
         inode->setNumHardlinksUnpersistent(--linkCount);

      inode->setIsInlined(false); // it's now definitely not inlined anymore

      if (!wasInlined) // if wasInlines == true -> disk inode already updated by dentry unlink code
         inode->updateInodeOnDisk(entryInfo);

      this->fileStore.releaseFileInode(inode);
   }

   /* Now link to the disposal-dir if required. If the inode was inlined into the dentry,
    * the dentry/inode unlink code already links to the disposal dir and we do not need to do
    * this work. */
   if (!wasInlined && linkCount == 0)
   {
      DirInode* disposalDir = app->getDisposalDir();

      std::string inodePath  = MetaStorageTk::getMetaInodePath(
         app->getEntriesPath()->getPathAsStrConst(), entryID);

      /* NOTE: If we are going to have another inode-format, than the current
       *       inode-inlined into the dentry, we need to add code for that here. */

      disposalDir->linkFileInodeToDir(inodePath, entryID); // use entryID as file name
      // ignore the return code here, as we cannot do anything about it anyway.
   }

}

/**
 * Reads all inodes from the given inodes storage hash subdir.
 *
 * Note: This is intended for use by Fsck only.
 *
 * Note: Offset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
 * Note: You have reached the end of the directory when "outEntryIDs.size() != maxOutNames".
 *
 *
 * @param hashDirNum number of a hash dir in the "entries" storage subdir
 * @param lastOffset zero-based offset; represents the native local fs offset;
 * @param outDirInodes the read directory inodes in the format fsck saves them
 * @param outFileInodes the read file inodes in the format fsck saves them
 * @param outNewOffset is only valid if return value indicates success.
 */
FhgfsOpsErr MetaStore::getAllInodesIncremental(unsigned hashDirNum, int64_t lastOffset,
   unsigned maxOutInodes, FsckDirInodeList* outDirInodes, FsckFileInodeList* outFileInodes,
   int64_t* outNewOffset)
{
   const char* logContext = "MetaStore (get all inodes inc)";

   App* app = Program::getApp();

   uint16_t rootNodeNumID = app->getMetaNodes()->getRootNodeNumID();
   uint16_t localNodeNumID = app->getLocalNode()->getNumID();

   StringList entryIDs;
   unsigned firstLevelHashDir;
   unsigned secondLevelHashDir;
   StorageTk::splitHashDirs(hashDirNum, &firstLevelHashDir, &secondLevelHashDir);

   FhgfsOpsErr readRes = getAllEntryIDFilesIncremental(firstLevelHashDir, secondLevelHashDir,
      lastOffset, maxOutInodes, &entryIDs, outNewOffset);

   if (readRes != FhgfsOpsErr_SUCCESS)
   {
      LogContext(logContext).logErr(
         "Failed to read inodes from hash dirs; HashDir Level 1: "
            + StringTk::uintToStr(firstLevelHashDir) + "; HashDir Level 2: "
            + StringTk::uintToStr(firstLevelHashDir));
      return readRes;
   }

   // the actual entry processing
   for ( StringListIter entryIDIter = entryIDs.begin(); entryIDIter != entryIDs.end();
      entryIDIter++ )
   {
      std::string entryID = *entryIDIter;

      // now try to reference the file and see what we got
      FileInode* fileInode = NULL;
      DirInode* dirInode = NULL;

      DirEntryType dirEntryType = this->referenceInode(entryID, &fileInode, &dirInode);

      if ( (dirEntryType == DirEntryType_DIRECTORY) && (dirInode) )
      {
         // entry is a directory
         std::string parentDirID;
         uint16_t parentNodeID;
         dirInode->getParentInfo(&parentDirID, &parentNodeID);

         uint16_t ownerNodeID = dirInode->getOwnerNodeID();

         // in the unlikely case, that this is the root directory and this MDS is not the owner of
         // root ignore the entry
         if (unlikely( (entryID.compare(META_ROOTDIR_ID_STR) == 0)
            && rootNodeNumID != localNodeNumID) )
            continue;

         // not root => get stat data and create a FsckDirInode with data
         StatData statData;
         dirInode->getStatData(statData);

         UInt16Vector stripeTargets;
         FsckStripePatternType stripePatternType = FsckTk::stripePatternToFsckStripePattern(
            dirInode->getStripePattern(), &stripeTargets);

         FsckDirInode fsckDirInode(entryID, parentDirID, parentNodeID, ownerNodeID,
            statData.getFileSize(), statData.getNumHardlinks(), stripeTargets, stripePatternType,
            localNodeNumID);

         outDirInodes->push_back(fsckDirInode);

         this->releaseDir(entryID);
      }
      else if (fileInode)
      {
         // directory not successful => must be a file-like object

         // create a FsckFileInode with data
         std::string parentDirID;
         uint16_t parentNodeID;
         UInt16Vector stripeTargets;

         int mode;
         unsigned userID;
         unsigned groupID;

         int64_t fileSize;
         int64_t creationTime;
         int64_t modificationTime;
         int64_t lastAccessTime;
         unsigned numHardLinks;

         fileInode->getParentInfo(&parentDirID, &parentNodeID);

         StatData statData;

         if ( fileInode->getStatData(statData) == FhgfsOpsErr_SUCCESS )
         {
            mode = statData.getMode();
            userID = statData.getUserID();
            groupID = statData.getGroupID();
            fileSize = statData.getFileSize();
            creationTime = statData.getCreationTimeSecs();
            modificationTime = statData.getModificationTimeSecs();
            lastAccessTime = statData.getLastAccessTimeSecs();
            numHardLinks = statData.getNumHardlinks();
         }
         else
         {
            LogContext(logContext).logErr(std::string("Unable to stat file inode: ") +
               entryID + ". SysErr: " + System::getErrString() );
            mode = 0;
            userID = 0;
            groupID = 0;
            fileSize = 0;
            creationTime = 0;
            modificationTime = 0;
            lastAccessTime = 0;
            numHardLinks = 0;
         }

         StripePattern* stripePattern = fileInode->getStripePattern();
         unsigned chunkSize;
         FsckStripePatternType stripePatternType = FsckTk::stripePatternToFsckStripePattern(
            stripePattern, &stripeTargets, &chunkSize);

         FsckFileInode fsckFileInode(entryID, parentDirID, parentNodeID, mode, userID, groupID,
            fileSize, creationTime, modificationTime, lastAccessTime, numHardLinks, stripeTargets,
            stripePatternType, chunkSize, localNodeNumID);
         outFileInodes->push_back(fsckFileInode);

         // parentID is absolutely irrelevant here, because we know that this inode is not inlined
         this->releaseFile("", fileInode);
      }
      else // something went wrong with opening
      {
         // create a file inode as dummy (could also be a dir inode, but we chose fileinode)
         UInt16Vector stripeTargets;
         FsckDirInode fsckDirInode(entryID, "", 0, 0, 0, 0, stripeTargets,
            FsckStripePatternType_INVALID, localNodeNumID, false);
      }
   }

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Reads all raw entryID filenames from the given "inodes" storage hash subdirs.
 *
 * Note: This is intended for use by the FullRefresher component and Fsck.
 *
 * Note: Offset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
 * Note: You have reached the end of the directory when "outEntryIDs.size() != maxOutNames".
 *
 * @param hashDirNum number of a hash dir in the "entries" storage subdir
 * @param lastOffset zero-based offset; represents the native local fs offset;
 * @param outEntryIDFiles the raw filenames of the entries in the given hash dir (so you will need
 * to remove filename suffixes to use these as entryIDs).
 * @param outNewOffset is only valid if return value indicates success.
 *
 */
FhgfsOpsErr MetaStore::getAllEntryIDFilesIncremental(unsigned firstLevelhashDirNum,
   unsigned secondLevelhashDirNum, int64_t lastOffset, unsigned maxOutEntries,
   StringList* outEntryIDFiles, int64_t* outNewOffset)
{
   const char* logContext = "Inode (get entry files inc)";
   App* app = Program::getApp();

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   std::string path = StorageTkEx::getMetaInodeHashDir(
      app->getEntriesPath()->getPathAsStrConst(), firstLevelhashDirNum, secondLevelhashDirNum);


   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   DIR* dirHandle = opendir(path.c_str() );
   if(!dirHandle)
   {
      LogContext(logContext).logErr(std::string("Unable to open entries directory: ") +
         path + ". SysErr: " + System::getErrString() );

      goto err_unlock;
   }


   errno = 0; // recommended by posix (readdir(3p) )

   // seek to offset
   if(lastOffset)
      seekdir(dirHandle, lastOffset); // (seekdir has no return value)

   // the actual entry reading
   for( ; (numEntries < maxOutEntries) && (dirEntry = StorageTk::readdirFiltered(dirHandle) );
       numEntries++)
   {
      outEntryIDFiles->push_back(dirEntry->d_name);
      *outNewOffset = dirEntry->d_off;
   }

   if(!dirEntry && errno)
   {
      LogContext(logContext).logErr(std::string("Unable to fetch entries directory entry from: ") +
         path + ". SysErr: " + System::getErrString() );
   }
   else
   { // all entries read
      retVal = FhgfsOpsErr_SUCCESS;
   }


   closedir(dirHandle);

err_unlock:
   safeLock.unlock(); // U N L O C K

   return retVal;
}

void MetaStore::getReferenceStats(size_t* numReferencedDirs, size_t* numReferencedFiles)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   *numReferencedDirs = dirStore.getSize();
   *numReferencedFiles = fileStore.getSize();

   // TODO: walk through dirStore and get for all dirs the fileStore size

   safeLock.unlock(); // U N L O C K
}

void MetaStore::getCacheStats(size_t* numCachedDirs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   *numCachedDirs = dirStore.getCacheSize();

   safeLock.unlock(); // U N L O C K
}

/**
 * Asynchronous cache sweep.
 *
 * @return true if a cache flush was triggered, false otherwise
 */
bool MetaStore::cacheSweepAsync()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   bool retVal = dirStore.cacheSweepAsync();

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Check and sleep if there is a rmdir of dirID going on
 *
 * Note: If we had to sleep, we are going to possibly convert a read-lock into a write-lock
 *
 * @param isReadLock - do we have read-lock or write-lock on this->rwLock
 * @return FhgfsOpsErr_SUCCESS - no rmdir or rmdir not successful
 *         FhgfsOpsErr_PATHNOTEXISTS - rmdir successful. So this thread raced with the rmdir
 *                                     process and it can safely return to the client
 *                                     ENOENT
 */
FhgfsOpsErr MetaStore::checkRmDirMap(std::string dirID)
{
   std::string logContext = "Check if current dir is to be deleted by another thread.";
   RmDirMapIter iter = this->rmDirMap.find(dirID);

   while (true)
   {
      if (likely(iter == this->rmDirMap.end()) )
         return FhgfsOpsErr_SUCCESS; // nothing to do

      // FIXME Bernd: Write a unit test, to verify the code below really works.

      /* If we end here, we are in the (hopefully) very rare situation that one thread deletes
       * a directory and another thread tries to do sub-dir operations (stat/create/delete fileStore).
       * Or this thread again tries to delete that directory.
       */
      RmDirMapValues* values = iter->second;
      values->refCounter.increase();

      /* We should end here very rarely at all, so we can afford to do sanity checks even
       * in non-debug compilations. */
      bool isReadLock = this->rwlock.isROLocked();
      if (unlikely(!isReadLock && this->rwlock.isRWLocked() ) )
      {
         LogContext(logContext).logErr("Bug: MetaStore not locked.");
         LogContext(logContext).logBacktrace();
      }

      this->rwlock.unlock(); // unlock the metaStore lock before taking the mutex lock

      /* at this point we gave up all locks and introduced a possible race, which is the reason
       * for the outer while (true) loop */

      values->mutex.lock();

      // when we are here the rmdir() thread released the mutex lock

      values->refCounter.decrease();

      bool rmDirSuccess = values->rmDirSuccess;

      /* we are protected (serialized) by the mutex here, so the last thread can safely delete
       * the object. Note: Make sure the rmdir-thread already removed the id from the map before
       * unlocking the mutex, as we must ensure no new threads reference the object */
      bool shallDelete = (values->refCounter.read() == 0) ? true : false;

      values->mutex.unlock();

      if (shallDelete)
         delete values;

      // restore the old lock
      if (isReadLock)
         this->rwlock.readLock();
      else
         this->rwlock.writeLock();

      if (rmDirSuccess)
         return FhgfsOpsErr_PATHNOTEXISTS; // we raced with an rmdir() and rmdir() won

      /* So rmdir() failed and we can continue with our operation in the directory.
       * As we have given up the metaStore lock, we need to re-get it and again need to check for
       * possible other rmdir() threads.
       */

      iter = this->rmDirMap.find(dirID);
   }
}

/**
 * So we failed to delete chunk files and need to create a new disposal file for later cleanup.
 *
 * @param inode will be deleted (or owned by another object) no matter whether this succeeds or not
 *
 * Note: No MetaStore lock required, as the disposal dir cannot be removed.
 */
FhgfsOpsErr MetaStore::insertDisposableFile(FileInode* inode)
{
   LogContext log("MetaStore (insert disposable file)");

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   DirInode* disposalDir = Program::getApp()->getDisposalDir();

   StripePattern* stripePattern = inode->getStripePatternAndSetToNull();

   MkFileDetails mkDetails(inode->getEntryID(), inode->getUserID(), inode->getGroupID(),
      inode->getMode() );

   FhgfsOpsErr retVal = metaStore->mkMetaFile(disposalDir, &mkDetails, NULL, stripePattern, NULL,
      NULL);
   if (retVal != FhgfsOpsErr_SUCCESS)
      log.log(2, std::string("Failed to create disposal file, for id: ") + inode->getEntryID() +
         "Storage chunks will not be entirely deleted!");

   delete inode;

   return retVal;
}
