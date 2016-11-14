#include <common/threading/SafeMutexLock.h>
#include <program/Program.h>
#include "InodeFileStore.h"


/**
 * check if the given ID is in the store
 *
 */
bool InodeFileStore::isInStore(const std::string& fileID)
{
   bool inStore = false;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   InodeMapIter iter = inodes.find(fileID);
   if(iter != inodes.end() )
      inStore = true;

   safeLock.unlock(); // U N L O C K

   return inStore;
}

/**
 * Get the referencer and delete this ID from the map. Mainly used to move the referencer between
 * Stores.
 */
FileInodeReferencer* InodeFileStore::getReferencerAndDeleteFromMap(const std::string& fileID)
{
   FileInodeReferencer* fileRefer = NULL;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   InodeMapIter iter = this->inodes.find(fileID);

   if(iter != inodes.end() )
   { // exists in map
      fileRefer = iter->second;

      this->inodes.erase(iter);
   }

   safeLock.unlock(); // U N L O C K

   return fileRefer;

}

/**
 * References a file to be known to already referenced.
 * Also could be called "referenceReferencedFile"
 *
 * Note: remember to call releaseFileInode()
 *
 * @param loadFromDisk - true for the per-directory InodeFileStore, false for references
 *                       from MetaStore (global map)
 * @return NULL if no such file exists
 */
FileInode* InodeFileStore::referenceLoadedFile(const std::string& entryID)
{
   FileInode* inode = NULL;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   InodeMapIter iter = this->inodes.find(entryID);

   if(iter != this->inodes.end() )
   {
      inode = referenceFileInodeMapIterUnlocked(iter, &this->inodes);
   }

   safeLock.unlock(); // U N L O C K

   return inode;
}


/**
 * Note: remember to call releaseFileInode()
 *
 * @param loadFromDisk - true for the per-directory InodeFileStore, false for references
 *                       from MetaStore (global map)
 * @return NULL if no such file exists
 *
 * TODO: Does not need writeLock if loadFromDisk=false and nothing found
 */
FileInode* InodeFileStore::referenceFileInode(EntryInfo* entryInfo, bool loadFromDisk)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FileInode* inode = referenceFileInodeUnlocked(entryInfo, loadFromDisk);

   safeLock.unlock(); // U N L O C K

   return inode;
}

/**
 * Note: this->rwlock needs to be write locked
 * Note: We do not add a reference if isRename == true, but we set an exclusive flag and just
 *       return an unreferenced inode, which can be deleted anytime.
 */
FileInode* InodeFileStore::referenceFileInodeUnlocked(EntryInfo* entryInfo, bool loadFromDisk)
{
   FileInode* inode = NULL;

   InodeMapIter iter =  this->inodes.find(entryInfo->getEntryID() );

   if(iter == this->inodes.end() && loadFromDisk)
   { // not in map yet => try to load it
      loadAndInsertFileInodeUnlocked(entryInfo, iter);
   }

   if(iter != this->inodes.end() )
   { // outInode exists
      inode = referenceFileInodeMapIterUnlocked(iter, &this->inodes);
   }

   return inode;
}

/**
 * Return an unreferenced inode object. The inode is also not exclusively locked.
 */
FhgfsOpsErr InodeFileStore::getUnreferencedInodeUnlocked(EntryInfo* entryInfo, FileInode*& outInode)
{
   FileInode* inode = NULL;
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;

   InodeMapIter iter =  this->inodes.find(entryInfo->getEntryID() );

   if(iter == this->inodes.end() )
   { // not in map yet => try to load it.
      loadAndInsertFileInodeUnlocked(entryInfo, iter);
   }

   if(iter != this->inodes.end() )
   { // outInode exists => check whether no references etc. exist
      FileInodeReferencer* inodeRefer = iter->second;
      inode = inodeRefer->getReferencedObject();

      if(inodeRefer->getRefCount() ||
        (inode->getExclusiveThreadID() && inode->getExclusiveThreadID() != System::getPosixTID() ) )
      {
         retVal = FhgfsOpsErr_INUSE;
         inode = NULL;

         /* note: We do not unload the outInode here (if we loaded it in this method),
          *       because freshly loaded inodes can't be referenced or exclusive and hence
          *       they cannot be "in use".  */
      }
      else
         retVal = FhgfsOpsErr_SUCCESS;
   }

   if (inode && inode->getNumHardlinks() > 1)
   {  /* So the inode is not referenced and we set our exclusive lock. However, there are several
       * hardlinks for this file. Currently only rename with a linkCount == 1 is supported! */
      deleteUnreferencedInodeUnlocked(entryInfo->getEntryID() );
      inode = NULL;
      retVal = FhgfsOpsErr_INUSE;
   }

   outInode = inode;
   return retVal;
}

/**
 * referece an a file from InodeMapIter
 * NOTE: iter should have been checked by the caller: iter != this->inodes.end()
 */
FileInode* InodeFileStore::referenceFileInodeMapIterUnlocked(InodeMapIter& iter, InodeMap* files)
{
   const char* logContext = "Reference File Inode";
   FileInode* inode = NULL;

   if (unlikely(iter == files->end() ) )
      return inode;

   FileInodeReferencer* inodeRefer = iter->second;
   FileInode* inodeNonRef = inodeRefer->getReferencedObject();

   if(!inodeNonRef->getExclusiveThreadID() ||
      inodeNonRef->getExclusiveThreadID() == System::getPosixTID() )
   {
      inode = inodeRefer->reference();
      // note: we don't need to check unload here, because exclusive means there already is a
      // reference so we definitely didn't load here

      LOG_DEBUG(logContext, Log_SPAM,  std::string("FileInodeID: ")        +
         iter->first                                                       +
         " Refcount: " + StringTk::intToStr(inodeRefer->getRefCount() ) );
      IGNORE_UNUSED_VARIABLE(logContext);
   }

   return inode;
}

/**
 * Decrease the inode reference counter using the given iter.
 *
 * Note: InodeFileStore needs to be write-locked.
 *
 * @return number of inode references after release()
 */
unsigned InodeFileStore::decreaseInodeRefCountUnlocked(InodeMapIter& iter)
{
   const char* logContext = "Release File Inode";

   // decrease refount
   FileInodeReferencer* inodeRefer = iter->second;

   unsigned refCount = (unsigned) inodeRefer->release();

   LOG_DEBUG(logContext, Log_SPAM,  std::string("FileInodeID: ") + iter->first +
      " Refcount: " + StringTk::intToStr(inodeRefer->getRefCount() ) );
   IGNORE_UNUSED_VARIABLE(logContext);

   if(!refCount)
   { // dropped last reference => unload outInode
      delete(inodeRefer);
      this->inodes.erase(iter);
   }


   return refCount;
}

/**
 * Close the given file. Also updates the InodeFile on disk.
 */
bool InodeFileStore::closeFile(EntryInfo* entryInfo, FileInode* inode, unsigned accessFlags,
   unsigned* outNumHardlinks, unsigned* outNumRefs)
{
   // TODO: Test with a read-lock if the file is in the store at all?

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool inStore = false;
   *outNumHardlinks = 1; // (we're careful here about inodes that are not currently open)

   InodeMapIter iter = this->inodes.find(inode->getEntryID() );
   if(iter != this->inodes.end() )
   { // outInode exists

      *outNumHardlinks = inode->getNumHardlinks();

      // Store inode information on disk, they have been set with inode->setDynAttribs() before
      entryInfo->setInodeInlinedFlag(inode->getIsInlined() );
      inode->decNumSessionsAndStore(entryInfo, accessFlags);

      *outNumRefs = decreaseInodeRefCountUnlocked(iter);

      inStore = true;
   }

   safeLock.unlock(); // U N L O C K

   return inStore;
}

/**
 * @param outNumHardlinks for quick on-close unlink check (may not be NULL!)
 * @return false if file was not in store at all, true if we found the file in the store
 */
bool InodeFileStore::releaseFileInode(FileInode* inode)
{
   bool inStore = false;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   InodeMapIter iter = this->inodes.find(inode->getEntryID() );
   if(iter != this->inodes.end() )
   { // outInode exists => decrease refCount
      decreaseInodeRefCountUnlocked(iter);
      inStore = true;
   }

   safeLock.unlock(); // U N L O C K

   return inStore;
}

/**
 * Check whether the file is unlinkable (not in use).
 * Can either be used with a reference or a fileID.
 *
 * @return FhgfsOpsErr_SUCCESS when not in use, FhgfsOpsErr_INUSE when the inode is referenced and
 *    FhgfsOpsErr_PATHNOTEXISTS when it is exclusively locked.
 */
FhgfsOpsErr InodeFileStore::isUnlinkableUnlocked(EntryInfo* entryInfo)
{
   FileInode* inode;

   std::string entryID = entryInfo->getEntryID();

   FhgfsOpsErr delErr = FhgfsOpsErr_SUCCESS;

   InodeMapCIter iter = inodes.find(entryID);
   if(iter != inodes.end() )
   {
      FileInodeReferencer* fileRefer = iter->second;
      inode = fileRefer->getReferencedObject();

      if(fileRefer->getRefCount() )
         delErr = FhgfsOpsErr_INUSE;
      else
      if(inode->getExclusiveThreadID() && inode->getExclusiveThreadID() != System::getPosixTID() )
         delErr = FhgfsOpsErr_PATHNOTEXISTS; /* TODO: Not correct (if rename() fails), but will be
                                              *       FIXED using the per-file checkMap holds. */
   }

   return delErr;
}

FhgfsOpsErr InodeFileStore::isUnlinkable(EntryInfo* entryInfo)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr retVal = this->isUnlinkableUnlocked(entryInfo);

   safeLock.unlock();

   return retVal;
}

/**
 * @param outInode will be set to the unlinked file and the object must then be deleted by the
 * caller (can be NULL if the caller is not interested in the file)
 *
 * TODO: It is pointless to return any errors if outInode cannot be created. We can simply
 *              continue. If there are storage objects, we cannot delete them one way or the
 *              other. So no need to return an error here. Update all code path to handle
 *              *outInode = NULL, even is the return code is FhgfsOpsErr_SUCCESS
 */
FhgfsOpsErr InodeFileStore::unlinkFileInodeUnlocked(EntryInfo* entryInfo, FileInode** outInode)
{
   if(outInode)
      *outInode = NULL;

   std::string entryID = entryInfo->getEntryID();

   FhgfsOpsErr unlinkableRes = isUnlinkableUnlocked(entryInfo);
   if(unlinkableRes != FhgfsOpsErr_SUCCESS)
      return unlinkableRes;

   if (outInode)
   {
      *outInode = createUnreferencedInodeUnlocked(entryInfo);
      if (!*outInode)
         return FhgfsOpsErr_PATHNOTEXISTS;
   }

   bool unlinkRes = FileInode::unlinkStoredInodeUnlocked(entryID, entryInfo->getIsBuddyMirrored());
   if(!unlinkRes)
   {
      if(outInode)
      {
         delete(*outInode);
         *outInode = NULL;
      }

      return FhgfsOpsErr_INTERNAL;
   }

   return FhgfsOpsErr_SUCCESS;
}

/**
 * @param outFile will be set to the unlinked file and the object must then be deleted by the caller
 * (can be NULL if the caller is not interested in the file)
 */
FhgfsOpsErr InodeFileStore::unlinkFileInode(EntryInfo* entryInfo, FileInode** outInode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr delErr = unlinkFileInodeUnlocked(entryInfo, outInode);

   safeLock.unlock(); // U N L O C K

   return delErr;
}

/**
 * Note: This works by serializing the original and marking the object unreferencable (exclusive),
 * so remember to call movingCancel() or movingComplete()
 *
 * @param buf target buffer for serialization (only valid if success is returned)
 * @param bufLen must be at least META_SERBUF_SIZE
 * @param outUsedBufLen the used bufLen
 */
FhgfsOpsErr InodeFileStore::moveRemoteBegin(EntryInfo* entryInfo, char* buf, size_t bufLen,
   size_t* outUsedBufLen)
{
   const char* logContext = "Serialize Inode";
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;

   if(bufLen < META_SERBUF_SIZE)
   {
      LogContext(logContext).log(Log_ERR, "Error: Buffer too small!");
      return FhgfsOpsErr_INTERNAL;
   }

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FileInode* inode;
   retVal = getUnreferencedInodeUnlocked(entryInfo, inode); // does not set refCount
   if (retVal == FhgfsOpsErr_SUCCESS)
   {
      /* We got an inode, which is in the map, but is unreferenced. Now we are going to exclusively
       * lock it. If another thread should try to reference it, it will fail due to this lock.
       */
      inode->setExclusiveTID(System::getPosixTID() );

      /* Set the origParentEntryID before serializing and moving the inode / file. It is not set
       * by default to avoid additional diskData for most files.
       */
      inode->setPersistentOrigParentID(entryInfo->getParentEntryID() );

      Serializer ser(buf, bufLen);
      inode->serializeMetaData(ser);
      *outUsedBufLen = ser.size();
   }

   safeLock.unlock(); // U N L O C K

   return retVal;
}


/**
 * Finish the rename/move operation by deleting the inode object.
 */
void InodeFileStore::moveRemoteComplete(const std::string& entryID)
{
   // moving succeeded => delete original

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   deleteUnreferencedInodeUnlocked(entryID);

   safeLock.unlock(); // U N L O C K
}

/**
 * Finish the rename/move operation by deleting the inode object.
 */
void InodeFileStore::deleteUnreferencedInodeUnlocked(const std::string& entryID)
{
   InodeMapIter iter = inodes.find(entryID);
   if(iter != inodes.end() )
   { // file exists
      FileInodeReferencer* fileRefer = iter->second;

      delete fileRefer;
      inodes.erase(entryID);
   }
}

/**
 * Note: This does not load any entries, so it will only return the number of already loaded
 * entries. (Only useful for debugging and statistics probably.)
 */
size_t InodeFileStore::getSize()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   size_t filesSize = inodes.size();

   safeLock.unlock(); // U N L O C K

   return filesSize;
}

/**
 * Note: Open inodes are always also implicitly referenced.
 *
 * @param accessFlags OPENFILE_ACCESS_... flags
 * @param loadFromDisk - true for access from DirInode, false for access from MetaStore
 */
FhgfsOpsErr InodeFileStore::openFile(EntryInfo* entryInfo, unsigned accessFlags,
   FileInode*& outInode, bool loadFromDisk)
{
   outInode = referenceFileInode(entryInfo, loadFromDisk);
   if (!outInode)
      return FhgfsOpsErr_PATHNOTEXISTS;

   outInode->incNumSessions(accessFlags);

   return FhgfsOpsErr_SUCCESS;
}

bool InodeFileStore::exists(std::string fileID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   bool existsRes = existsUnlocked(fileID);

   safeLock.unlock(); // U N L O C K

   return existsRes;
}

bool InodeFileStore::existsUnlocked(std::string fileID)
{
   App* app = Program::getApp();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(
      app->getInodesPath()->str(), fileID);

   bool existsRes = false;

   int accessRes = ::access(metaFilename.c_str(), F_OK);
   if(!accessRes)
      existsRes = true;

   return existsRes;

}

/**
 * get the statData of an inode
 *
 * @param loadFromDisk   if false and the inode is not referenced yet
 *                       we are not going to try to get data from disk.
 */
FhgfsOpsErr InodeFileStore::stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData)
{
   std::string entryID = entryInfo->getEntryID();
   FhgfsOpsErr statRes = FhgfsOpsErr_PATHNOTEXISTS;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   InodeMapIter iter = inodes.find(entryID);
   if(iter != inodes.end() )
   { // inode loaded
      FileInodeReferencer* fileRefer = iter->second;
      FileInode* inode = fileRefer->getReferencedObject();

      statRes = inode->getStatData(outStatData);

      safeLock.unlock(); // U N L O C K
   }
   else
   {
      safeLock.unlock(); // no need to keep the lock anymore

      if (loadFromDisk == true)
      {  // not loaded => static stat
         statRes = FileInode::getStatData(entryInfo, outStatData);
      }

   }

   return statRes;
}

/**
 * @param validAttribs SETATTR_CHANGE_...-Flags.
 */
FhgfsOpsErr InodeFileStore::setAttr(EntryInfo* entryInfo, int validAttribs,
   SettableFileAttribs* attribs)
{
   std::string entryID = entryInfo->getEntryID();
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   InodeMapIter iter = inodes.find(entryID);
   if(iter == inodes.end() )
   { // not loaded => load, apply, destroy

      // Note: A very uncommon code path, as SetAttrMsgEx::setAttr() references the inode first.

      FileInode* inode = FileInode::createFromEntryInfo(entryInfo);

      if(inode)
      { // loaded
         bool setRes = inode->setAttrData(entryInfo, validAttribs, attribs);

         if(likely(setRes) )
         { // attr update succeeded
            retVal = FhgfsOpsErr_SUCCESS;
         }
         else
            retVal = FhgfsOpsErr_INTERNAL;

         delete inode;
      }
   }
   else
   { // inode loaded
      FileInodeReferencer* inodeRefer = iter->second;
      FileInode* inode = inodeRefer->getReferencedObject();

      if(!inode->getExclusiveThreadID() || inode->getExclusiveThreadID() == System::getPosixTID() )
      {
         bool setRes = inode->setAttrData(entryInfo, validAttribs, attribs);

         if(likely(setRes) )
         { // attr update succeeded
            retVal = FhgfsOpsErr_SUCCESS;
         }
         else
            retVal = FhgfsOpsErr_INTERNAL;
      }
   }

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Loads a file and inserts it into the map.
 *
 * Note: Caller must make sure that the element wasn't in the map before.
 *
 * @return newElemIter only valid if true is returned, untouched otherwise
 */
bool InodeFileStore::loadAndInsertFileInodeUnlocked(EntryInfo* entryInfo, InodeMapIter& newElemIter)
{
   FileInode* inode = FileInode::createFromEntryInfo(entryInfo);
   if(!inode)
      return false;

   std::string entryID = entryInfo->getEntryID();
   newElemIter = inodes.insert(InodeMapVal(entryID, new FileInodeReferencer(inode) ) ).first;

   return true;
}

/**
 * Insert the existing FileInodeReferencer into the map.
 *
 * This is mainly required to move references between stores (per-directory to metaStore)
 */
bool InodeFileStore::insertReferencer(std::string entryID, FileInodeReferencer* fileRefer)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool retVal = this->inodes.insert(InodeMapVal(entryID, fileRefer) ).second;

   safeLock.unlock(); // U N L O C K

   return retVal;
}


void InodeFileStore::clearStoreUnlocked()
{
   const char* logContext = "FileStore::clearStoreUnlocked";
   App* app = Program::getApp();

   LOG_DEBUG(logContext, Log_DEBUG,
      std::string("# of loaded entries to be cleared: ") + StringTk::intToStr(inodes.size() ) );
   IGNORE_UNUSED_VARIABLE(logContext);

   for(InodeMapIter iter = inodes.begin(); iter != inodes.end(); iter++)
   {
      FileInode* file = iter->second->getReferencedObject();

      if(unlikely(file->getNumSessionsAll() ) )
      { // check whether file was still open
         LOG_DEBUG(logContext, Log_DEBUG,
            std::string("File was still open during shutdown: ") +
            file->getEntryID() + "; # of sessions: " +
            StringTk::intToStr(file->getNumSessionsAll() ) );

         if (!app->getSelfTerminate() )
            LogContext(logContext).logBacktrace();
      }

      delete(iter->second);
   }

   inodes.clear();
}

/**
 * Increase or decrease the link count of an inode by the given value
 */
FhgfsOpsErr InodeFileStore::incDecLinkCount(FileInode& inode, EntryInfo* entryInfo,
   uint16_t parentMirrorNodeID, int value)
{
   std::string entryID = entryInfo->getEntryID();
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;

   bool setRes = inode.incDecNumHardLinks(entryInfo, value);
   if(likely(setRes) )
   {  //  update succeeded
      retVal = FhgfsOpsErr_SUCCESS;
   }
   else
      retVal = FhgfsOpsErr_INTERNAL;

   return retVal;
}

