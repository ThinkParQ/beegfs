#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/Storagedata.h>
#include <common/threading/UniqueRWLock.h>
#include <common/toolkit/FsckTk.h>
#include <net/msghelpers/MsgHelperStat.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <storage/PosixACL.h>
#include <program/Program.h>
#include "MetaStore.h"

#include <sys/xattr.h>
#include <dirent.h>
#include <sys/types.h>

#include <boost/lexical_cast.hpp>

#define MAX_DEBUG_LOCK_TRY_TIME 30 // max lock wait time in seconds

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
DirInode* MetaStore::referenceDir(const std::string& dirID, const bool isBuddyMirrored,
   const bool forceLoad)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   return referenceDirUnlocked(dirID, isBuddyMirrored, forceLoad);
}

DirInode* MetaStore::referenceDirUnlocked(const std::string& dirID, bool isBuddyMirrored,
   bool forceLoad)
{
   return dirStore.referenceDirInode(dirID, isBuddyMirrored, forceLoad);
}

void MetaStore::releaseDir(const std::string& dirID)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   releaseDirUnlocked(dirID);
}

void MetaStore::releaseDirUnlocked(const std::string& dirID)
{
   dirStore.releaseDir(dirID);
}

/**
 * Reference a file. It is unknown if this file is already referenced in memory or needs to be
 * loaded. Therefore a complete entryInfo is required.
 */
MetaFileHandle MetaStore::referenceFile(EntryInfo* entryInfo)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   MetaFileHandle inode = referenceFileUnlocked(entryInfo);
   if (unlikely(!inode))
      return {};

   // Return MetaFileHandle from here if:
   // 1. The inode is inlined, or
   // 2. The inode is loaded into the global store, indicating that
   //    it was previously deinlined hence inlined=false was received from client, or
   // 3. Inode is already present in the global store (numParentRefs remains zero if inode
   //    is referenced from the global store)
   if (inode->getIsInlined() || (inode->getNumParentRefs() == 0))
      return inode;

   // If we reach at this point, then following holds true:
   // 1. The file inode is non-inlined (validated from on-disk metadata)
   // 2. The entryInfo received from client has stale value of inlined flag (i.e. true)
   // 3. The file inode is referenced in dir-specific store
   //
   // According to existing design, non-inlined file inodes should always be referenced
   // in the global inode store. Therefore, we need to perform following additional steps
   // to maintain consistency with aformentioned design:
   // 1. Take parent directory reference and release inode from dir-specific store
   // 2. Release the meta read-lock and call tryReferenceFileWriteLocked() to put inode
   //    reference into the global store

   auto dir = referenceDirUnlocked(entryInfo->getParentEntryID(),
      entryInfo->getIsBuddyMirrored(), false);
   if (!dir)
      return {};

   UniqueRWLock subDirLock(dir->rwlock, SafeRWLock_READ);

   inode->decParentRef();
   dir->fileStore.releaseFileInode(inode.get());

   subDirLock.unlock();
   releaseDirUnlocked(dir->getID());

   releaseDirUnlocked(dir->getID());

   lock.unlock(); // U N L O C K
   return tryReferenceFileWriteLocked(entryInfo);
}

/**
 * See referenceFileInode() for details. We already have the lock here.
 */
MetaFileHandle MetaStore::referenceFileUnlocked(EntryInfo* entryInfo)
{
   // load inode into global store from disk if it is nonInlined
   bool loadFromDisk = !entryInfo->getIsInlined();
   FileInode* inode = this->fileStore.referenceFileInode(entryInfo, loadFromDisk);

   if (inode)
      return {inode, nullptr};

   // not in global map, now per directory and also try to load from disk

   const std::string& parentEntryID = entryInfo->getParentEntryID();
   bool isBuddyMirrored = entryInfo->getIsBuddyMirrored();
   DirInode* subDir = referenceDirUnlocked(parentEntryID, isBuddyMirrored, false);

   if (!subDir)
      return {};

   UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_READ);
   inode = subDir->fileStore.referenceFileInode(entryInfo, true);

   if (!inode)
   {
      subDirLock.unlock();
      releaseDirUnlocked(entryInfo->getParentEntryID());
      return {};
   }

   // we are not going to release the directory here, as we are using its FileStore
   inode->incParentRef(parentEntryID);

   return {inode, subDir};
}

/**
 * See referenceFileInode() for details. DirInode is already known here and will not be futher
 * referenced due to the write-lock hold by the caller. Getting further references might cause
 * dead locks due to locking order problems (DirStore needs to be locked while holding the DirInode
 * lock).
 *
 * Locking:
 *    MetaStore read locked
 *    DirInode write locked.
 *
 * Note: Callers must release FileInode before releasing DirInode! Use this method with care!
 *
 */
MetaFileHandle MetaStore::referenceFileUnlocked(DirInode& subDir, EntryInfo* entryInfo)
{
   // load inode into global store from disk if it is nonInlined
   bool loadFromDisk = !entryInfo->getIsInlined();
   FileInode* inode = this->fileStore.referenceFileInode(entryInfo, loadFromDisk);
   if (inode)
      return {inode, nullptr};

   // not in global map, now per directory and also try to load from disk
   inode = subDir.fileStore.referenceFileInode(entryInfo, true);
   return {inode, nullptr};
}

/**
 * Reference a file known to be already referenced. So disk-access is not required and we don't
 * need the complete EntryInfo, but parentEntryID and entryID are sufficient.
 * Another name for this function also could be "referenceReferencedFiles".
 */
MetaFileHandle MetaStore::referenceLoadedFile(const std::string& parentEntryID,
   bool parentIsBuddyMirrored, const std::string& entryID)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   return referenceLoadedFileUnlocked(parentEntryID, parentIsBuddyMirrored, entryID);
}

/**
 * See referenceLoadedFile() for details. We already have the lock here.
 */
MetaFileHandle MetaStore::referenceLoadedFileUnlocked(const std::string& parentEntryID,
   bool isBuddyMirrored, const std::string& entryID)
{
   FileInode* inode = this->fileStore.referenceLoadedFile(entryID);
   if (inode)
      return {inode, nullptr};

   // not in global map, now per directory and also try to load from disk
   DirInode* subDir = referenceDirUnlocked(parentEntryID, isBuddyMirrored, false);
   if (!subDir)
      return {};

   UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_READ);
   inode = subDir->fileStore.referenceLoadedFile(entryID);

   if (!inode)
   {
      subDirLock.unlock();
      releaseDirUnlocked(parentEntryID);
      return {};
   }

   // we are not going to release the directory here, as we are using its FileStore
   inode->incParentRef(parentEntryID);

   return {inode, subDir};
}

/**
 * See referenceFileInode() for details. DirInode is already known here and will not be futher
 * referenced due to the write-lock hold by the caller. Getting further references might cause
 * dead locks due to locking order problems (DirStore needs to be locked while holding the DirInode
 * lock).
 *
 * Locking:
 *    MetaStore read locked
 *    DirInode write locked.
 *
 * Note: Callers must release FileInode before releasing DirInode! Use this method with care!
 *
 */
MetaFileHandle MetaStore::referenceLoadedFileUnlocked(DirInode& subDir, const std::string& entryID)
{
   FileInode* inode = this->fileStore.referenceLoadedFile(entryID);
   if (inode)
      return {inode, nullptr};

   // not in global map, now per directory and also try to load from disk
   inode = subDir.fileStore.referenceLoadedFile(entryID);
   return {inode, nullptr};
}

/**
 * Tries to reference the file inode with MetaStore's write lock held.
 * Moves the reference to the global store if not already present.
 *
 * @param entryInfo The entry information of the file.
 * @return A handle to the file inode if successful, otherwise an empty handle.
 */
MetaFileHandle MetaStore::tryReferenceFileWriteLocked(EntryInfo* entryInfo)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   if (!this->fileStore.isInStore(entryInfo->getEntryID()))
   {
      this->moveReferenceToMetaFileStoreUnlocked(entryInfo->getParentEntryID(),
         entryInfo->getIsBuddyMirrored(), entryInfo->getEntryID());
   }

   FileInode* inode = this->fileStore.referenceFileInode(entryInfo, true);
   if (inode)
      return {inode, nullptr};

   return {};
}

/**
 * Tries to open the file with MetaStore's write lock held.
 * Moves the reference to the global store if not present.
 *
 * @param entryInfo The entry information of the file.
 * @param accessFlags The access flags for opening the file.
 * @param outInode Output parameter for the file inode handle.
 * @return Error code indicating the result of the operation.
 */
FhgfsOpsErr MetaStore::tryOpenFileWriteLocked(EntryInfo* entryInfo, unsigned accessFlags, MetaFileHandle& outInode)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   if (!this->fileStore.isInStore(entryInfo->getEntryID()))
   {
      this->moveReferenceToMetaFileStoreUnlocked(entryInfo->getParentEntryID(),
         entryInfo->getIsBuddyMirrored(), entryInfo->getEntryID());
   }

   FileInode* inode;
   auto openRes = this->fileStore.openFile(entryInfo, accessFlags, inode, true);
   outInode = {inode, nullptr};
   return openRes;
}

bool MetaStore::releaseFile(const std::string& parentEntryID, MetaFileHandle& inode)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   return releaseFileUnlocked(parentEntryID, inode);
}

/**
 * Release a file inode.
 *
 * Note: If the inode belongs to the per-directory file store also this directory will be
 *       released.
 */
bool MetaStore::releaseFileUnlocked(const std::string& parentEntryID, MetaFileHandle& inode)
{
   bool releaseRes = fileStore.releaseFileInode(inode.get());
   if (releaseRes)
      return true;

   // Not in global map, now per directory and also try to load from disk.

   // NOTE: we assume here, that if the file inode is buddy mirrored, the parent is buddy
   // mirrored, to
   DirInode* subDir = referenceDirUnlocked(parentEntryID, inode->getIsBuddyMirrored(), false);
   if (!subDir)
      return false;

   UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_READ);

   inode->decParentRef();
   releaseRes = subDir->fileStore.releaseFileInode(inode.get());

   // this is the current lock and reference
   subDirLock.unlock();
   releaseDirUnlocked(parentEntryID);

   // when we referenced the inode we did not release the directory yet, so do that here
   releaseDirUnlocked(parentEntryID);

   return releaseRes;
}

/**
 * Release a file inode. DirInode is known by the caller. Only call this after using
 * referenceFileUnlocked(DirInode* subDir, EntryInfo* entryInfo)
 * or
 * referenceLoadedFileUnlocked(DirInode* subDir, std::string entryID)
 * to reference a FileInode.
 *
 * Locking:
 *    MetaStore is locked
 *    DirInode  is locked
 *
 * Note: No extra DirInode releases here, as the above mentioned referenceFileUnlocked() method also
 *       does not get additional references.
 */
bool MetaStore::releaseFileUnlocked(DirInode& subDir, MetaFileHandle& inode)
{
   bool releaseRes = fileStore.releaseFileInode(inode.get());
   if (releaseRes)
      return true;

   // Not in global map, now per directory.
   return subDir.fileStore.releaseFileInode(inode.get());
}


/*
 * Can be used to reference an inode, if it is unknown whether it is a file or directory. While
 * one of the out.... pointers will hold a reference to the corresponidng inode (which must be
 * released), the other pointer will be set to NULL.
 *
 * Note: This funtion is mainly used by fsck
 * Note: dirInodes will always be loaded from disk with this function
 * Note: This will NOT work with inlined file inodes, as we only pass the ID
 * Note: This is not very efficient, but OK for now. Can definitely be optimized.
 *
 * @param entryID
 * @param outDirInode
 * @param outFilenode
 *
 * @return false if entryID could neither be loaded as dir nor as file inode
 */
bool MetaStore::referenceInode(const std::string& entryID, bool isBuddyMirrored,
   MetaFileHandle& outFileInode, DirInode*& outDirInode)
{
   outFileInode = {};
   outDirInode  = NULL;

   // trying dir first, because we assume there are more non-inlined dir inodes than file inodes
   outDirInode = referenceDir(entryID, isBuddyMirrored, true);

   if (outDirInode)
      return true;

   // opening as dir failed => try as file

   /* we do not set full entryInfo (we do not have most of the info), but only entryID. That's why
      it does not work with inlined inodes */
   EntryInfo entryInfo(NumNodeID(), "", entryID, "", DirEntryType_REGULARFILE,0);
   if (isBuddyMirrored)
      entryInfo.setBuddyMirroredFlag(true);

   outFileInode = referenceFile(&entryInfo);

   return outFileInode;
}


FhgfsOpsErr MetaStore::isFileUnlinkable(DirInode& subDir, EntryInfo *entryInfo)
{
   FhgfsOpsErr isUnlinkable = this->fileStore.isUnlinkable(entryInfo);
   if (isUnlinkable == FhgfsOpsErr_SUCCESS)
      isUnlinkable = subDir.fileStore.isUnlinkable(entryInfo);

   return isUnlinkable;
}


/**
 * Move a fileInode reference from subDir->fileStore to (MetaStore) this->fileStore
 *
 * @return true if an existing reference was moved
 *
 * Note: MetaStore needs to be write locked!
 */
bool MetaStore::moveReferenceToMetaFileStoreUnlocked(const std::string& parentEntryID,
   bool parentIsBuddyMirrored, const std::string& entryID)
{
   bool retVal = false;
   const char* logContext = "MetaStore (Move reference from per-Dir fileStore to global Store)";

   DirInode* subDir = referenceDirUnlocked(parentEntryID, parentIsBuddyMirrored, false);
   if (unlikely(!subDir) )
       return false;

   UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_WRITE);

   FileInodeReferencer* inodeRefer = subDir->fileStore.getReferencerAndDeleteFromMap(entryID);
   if (likely(inodeRefer) )
   {
      // The inode is referenced in the per-directory fileStore. Move it to the global store.

      FileInode* inode = inodeRefer->reference();
      ssize_t numParentRefs = inode->getNumParentRefs();

      retVal = this->fileStore.insertReferencer(entryID, inodeRefer);
      if (unlikely(retVal == false) )
      {
         std::string msg = "Bug: Failed to move to MetaStore FileStore - already exists in map"
            " ParentID: " + parentEntryID + " EntryID: " + entryID;
         LogContext(logContext).logErr(msg);
         /* NOTE: We are leaking memory here, but as there is a general bug, this is better
          *       than trying to free an object possibly in-use. */
      }
      else
      {
         while (numParentRefs > 0)
         {
            releaseDirUnlocked(parentEntryID); /* inode references also always keep a dir reference,
                                                * so we need to release the dir as well */
            numParentRefs--;
            inode->decParentRef();
         }

         inodeRefer->release();
      }
   }
   else
   {
      retVal = true; /* it is probably a valid race (), so do not return an error,
                      * unlinkInodeLaterUnlocked() also can handle it as it does find this inode
                      * in the global FileStore then */
   }

   subDirLock.unlock();
   releaseDirUnlocked(parentEntryID);

   return retVal;
}

/**
 * @param accessFlags OPENFILE_ACCESS_... flags
 */
FhgfsOpsErr MetaStore::openFile(EntryInfo* entryInfo, unsigned accessFlags,
   MetaFileHandle& outInode, bool checkDisposalFirst)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   // session restore must load disposed files with disposal as parent entry id - if the file is
   // disposed any other parent id will also work, but will make that parent id unremovable for
   // as long as the file is opened.
   if (unlikely(checkDisposalFirst))
   {
      auto eiDisposal = *entryInfo;
      eiDisposal.setParentEntryID(META_DISPOSALDIR_ID_STR);

      FileInode* inode;
      auto openRes = this->fileStore.openFile(&eiDisposal, accessFlags, inode, true);
      if (inode)
      {
         outInode = {inode, nullptr};
         return openRes;
      }
   }

   /* check the MetaStore fileStore map here, but most likely the file will not be in this map,
    * but is either in the per-directory-map or has to be loaded from disk */
   if (this->fileStore.isInStore(entryInfo->getEntryID()))
   {
      FileInode* inode;
      auto openRes = this->fileStore.openFile(entryInfo, accessFlags, inode, false);
      outInode = {inode, nullptr};
      return openRes;
   }

   // onwards v7.4.0 non-inlined inode might be present due to hardlinks. Non inlined file inodes
   // should be referenced from and stored into global file store (and not in per-directory-store)
   // if inode is non-inlined then code block below will load inode from disk and puts it into
   // global store if not already present there
   //
   // to forceLoad inode into global file store we need to pass "loadFromDisk = true" in call to
   // InodeFileStore::openFile(...) below
   if (!entryInfo->getIsInlined())
   {
      FileInode* inode;
      auto openRes = this->fileStore.openFile(entryInfo, accessFlags, inode, true);
      outInode = {inode, nullptr};
      return openRes;
   }

   // not in global map, now per directory and also try to load from disk
   std::string parentEntryID = entryInfo->getParentEntryID();
   // Note: We assume, that if the file is buddy mirrored, the parent is mirrored, too
   bool isBuddyMirrored = entryInfo->getIsBuddyMirrored();

   DirInode* subDir = referenceDirUnlocked(parentEntryID, isBuddyMirrored, false);
   if (!subDir)
      return FhgfsOpsErr_INTERNAL;

   UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_READ);

   FileInode* inode;
   FhgfsOpsErr retVal = subDir->fileStore.openFile(entryInfo, accessFlags, inode, true);
   outInode = {inode, subDir};

   if (!outInode)
   {
      subDirLock.unlock();
      releaseDirUnlocked(parentEntryID);
      return retVal;
   }

   if (outInode->getIsInlined())
   {
      outInode->incParentRef(parentEntryID);
      // Do not release dir here, we are returning the inode referenced in subDirs fileStore!

      return retVal;
   }

   // If execution reaches this point then following holds true:
   // 1. The file inode is non-inlined (validated from on-disk metadata).
   // 2. The entryInfo received from client has stale value of inlined flag (i.e. true).
   // 3. The file inode reference exists in dir-specific store.
   //
   // According to existing design, non-inlined file inodes should always be referenced
   // in the global inode store. Therefore, the following additional steps are performed
   // to maintain consistency with the aforementioned design:
   // 1. Close the file in dir specific store and release parent directory reference
   // 2. Release meta read-lock and call tryOpenFileWriteLocked() to open the
   //    file in global store.

   unsigned numHardlinks; // ignored here!
   unsigned numInodeRefs; // ignored here!

   subDir->fileStore.closeFile(entryInfo, outInode.get(), accessFlags, &numHardlinks,
         &numInodeRefs);

   subDirLock.unlock();
   releaseDirUnlocked(parentEntryID);

   lock.unlock(); // U N L O C K
   return tryOpenFileWriteLocked(entryInfo, accessFlags, outInode);
}

/**
 * @param accessFlags OPENFILE_ACCESS_... flags
 * @param outNumHardlinks for quick on-close unlink check
 * @param outNumRef also for on-close unlink check
 */
void MetaStore::closeFile(EntryInfo* entryInfo, MetaFileHandle inode, unsigned accessFlags,
   unsigned* outNumHardlinks, unsigned* outNumRefs)
{
   const char* logContext = "Close file";
   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   // now release (and possible free (delete) the inode if not further referenced)

   // first try global metaStore map
   bool closeRes = this->fileStore.closeFile(entryInfo, inode.get(), accessFlags,
      outNumHardlinks, outNumRefs);
   if (closeRes)
      return;

   // not in global /store/map, now per directory

   // Note: We assume, that if the file is buddy mirrored, the parent is mirrored, too
   DirInode* subDir = referenceDirUnlocked(entryInfo->getParentEntryID(),
      entryInfo->getIsBuddyMirrored(), false);
   if (!subDir)
      return;

   UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_READ);

   inode->decParentRef(); /* Already decrease it here, as the inode might get destroyed
                           * in closeFile(). The counter is very important if there is
                           * another open reference on this file and if the file is unlinked
                           * while being open */

   closeRes = subDir->fileStore.closeFile(entryInfo, inode.get(), accessFlags,
      outNumHardlinks, outNumRefs);
   if (!closeRes)
   {
      LOG_DEBUG(logContext, Log_SPAM, "File not open: " + entryInfo->getEntryID() );
      IGNORE_UNUSED_VARIABLE(logContext);
   }

   subDirLock.unlock();
   releaseDirUnlocked(entryInfo->getParentEntryID());

   // we kept another dir reference in openFile(), so release it here
   releaseDirUnlocked(entryInfo->getParentEntryID());
}

/**
 * get statData of a DirInode or FileInode.
 *
 * @param outParentNodeID maybe NULL (default). Its value for FileInodes is always 0, as
 *    the value is undefined for files due to possible hard links.
 * @param outParentEntryID, as with outParentNodeID it is undefined for files
 */
FhgfsOpsErr MetaStore::stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData,
   NumNodeID* outParentNodeID, std::string* outParentEntryID)
{
   FhgfsOpsErr statRes = FhgfsOpsErr_PATHNOTEXISTS;

   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   if (entryInfo->getEntryType() == DirEntryType_DIRECTORY)
   { // entry is a dir
      return dirStore.stat(entryInfo->getEntryID(), entryInfo->getIsBuddyMirrored(), outStatData,
         outParentNodeID, outParentEntryID);
   }

   // entry is any type, but a directory, e.g. a regular file
   if (outParentNodeID)
   {  // no need to set these for a regular file
      *outParentNodeID = NumNodeID();
   }

   // first check if the inode is referenced in the global store
   // if inode is non-inlined then forceLoad it from disk if it is not
   // already present in global fileStore (similar to MetaStore::openFile())
   statRes = fileStore.stat(entryInfo, !entryInfo->getIsInlined(), outStatData);

   if (statRes != FhgfsOpsErr_PATHNOTEXISTS)
      return statRes;

   // not in global /store/map, now per directory

   // Note: We assume, that if the file is buddy mirrored, the parent is mirrored, too
   DirInode* subDir = referenceDirUnlocked(entryInfo->getParentEntryID(),
      entryInfo->getIsBuddyMirrored(), false);
   if (likely(subDir))
   {
      UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_READ);

      statRes = subDir->fileStore.stat(entryInfo, loadFromDisk, outStatData);

      subDirLock.unlock();
      releaseDirUnlocked(entryInfo->getParentEntryID());
   }

   return statRes;
}

FhgfsOpsErr MetaStore::setAttr(EntryInfo* entryInfo, int validAttribs, SettableFileAttribs* attribs)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   return setAttrUnlocked(entryInfo, validAttribs, attribs);
}

/**
 * @param validAttribs SETATTR_CHANGE_...-Flags or no flags to only update attribChangeTimeSecs
 * @param attribs  new attribs, may be NULL if validAttribs==0
 */
FhgfsOpsErr MetaStore::setAttrUnlocked(EntryInfo* entryInfo, int validAttribs,
   SettableFileAttribs* attribs)
{
   FhgfsOpsErr setAttrRes = FhgfsOpsErr_PATHNOTEXISTS;

   if (DirEntryType_ISDIR(entryInfo->getEntryType()))
      return dirStore.setAttr(entryInfo->getEntryID(), entryInfo->getIsBuddyMirrored(),
            validAttribs, attribs);

   if (this->fileStore.isInStore(entryInfo->getEntryID() ) )
      return this->fileStore.setAttr(entryInfo, validAttribs, attribs);

   // not in global /store/map, now per directory
   // NOTE: we assume that if the file is mirrored, the parent dir is mirrored too
   DirInode* subDir = referenceDirUnlocked(entryInfo->getParentEntryID(),
      entryInfo->getIsBuddyMirrored(), true);
   if (likely(subDir) )
   {
      UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_READ);

      setAttrRes = subDir->fileStore.setAttr(entryInfo, validAttribs, attribs);

      subDirLock.unlock();
      releaseDirUnlocked(entryInfo->getParentEntryID());
   }

   return setAttrRes;
}

FhgfsOpsErr MetaStore::incDecLinkCount(EntryInfo* entryInfo, int value)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);
   return incDecLinkCountUnlocked(entryInfo, value);
}

/**
 * Update link count value in file inode
 */
FhgfsOpsErr MetaStore::incDecLinkCountUnlocked(EntryInfo* entryInfo, int value)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;

   MetaFileHandle inode = referenceFileUnlocked(entryInfo);
   if (unlikely(!inode))
      return retVal;

   if (!inode->incDecNumHardLinks(entryInfo, value))
      retVal = FhgfsOpsErr_INTERNAL;
   else
      retVal = FhgfsOpsErr_SUCCESS;

   releaseFileUnlocked(entryInfo->getParentEntryID(), inode);
   return retVal;
}

/**
 * Set / update the parent information of a dir inode
 */
FhgfsOpsErr MetaStore::setDirParent(EntryInfo* entryInfo, NumNodeID parentNodeID)
{
   if ( unlikely(!DirEntryType_ISDIR(entryInfo->getEntryType() )) )
      return FhgfsOpsErr_INTERNAL;

   const std::string& dirID = entryInfo->getEntryID();
   const bool isBuddyMirrored = entryInfo->getIsBuddyMirrored();

   DirInode* dir = referenceDir(dirID, isBuddyMirrored, true);
   if ( !dir )
      return FhgfsOpsErr_PATHNOTEXISTS;

   // also update the time stamps
   FhgfsOpsErr setRes = dir->setDirParentAndChangeTime(entryInfo, parentNodeID);

   releaseDir(dirID);

   return setRes;
}

/**
 * Create a File (dentry + inlined-inode) from an existing inlined inode
 * Create a dentry (in Ver-3 format) from existing dentry (if inode was deinlined)
 *
 * @param dir         Already needs to be locked by the caller.
 * @param inode       Take values from this inode to create the new file. Object will be destroyed
 *                    here.
 */
FhgfsOpsErr MetaStore::mkMetaFileUnlocked(DirInode& dir, const std::string& entryName,
   EntryInfo* entryInfo, FileInode* inode)
{
   App* app = Program::getApp();
   Node& localNode = app->getLocalNode();
   DirEntryType entryType = entryInfo->getEntryType();
   const std::string& entryID = inode->getEntryID();

   NumNodeID ownerNodeID;
   if (entryInfo->getIsInlined())
   {
      ownerNodeID = inode->getIsBuddyMirrored() ?
         NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() ) : localNode.getNumID();
   }
   else
   {
      // owner node may not be same as local node for files having deinlined inode
      ownerNodeID = entryInfo->getOwnerNodeID();
   }

   DirEntry newDentry (entryType, entryName, entryID, ownerNodeID);

   if (inode->getIsBuddyMirrored())
      newDentry.setBuddyMirrorFeatureFlag();

   if (entryInfo->getIsInlined())
   {
      // only set inode data if we are dealing with inlined inode(s)
      FileInodeStoreData inodeDiskData(entryID, inode->getInodeDiskData() );
      inodeDiskData.setInodeFeatureFlags(inode->getFeatureFlags() );
      newDentry.setFileInodeData(inodeDiskData);
      inodeDiskData.setPattern(NULL); /* pattern now owned by newDentry, so make sure it won't be
                                       * deleted on inodeMetadata object destruction */
   }

   // create a dir-entry
   FhgfsOpsErr makeRes = dir.makeDirEntryUnlocked(&newDentry);

   delete inode;

   return makeRes;
}

/**
 * Create a new File (directory-entry with inlined inode)
 *
 * @param stripePattern     must be provided; will be assigned to given outInodeData.
 * @param outEntryInfo      will not be set if NULL (caller not interested)
 * @param outInodeData      will not be set if NULL (caller not interested)
 */
FhgfsOpsErr MetaStore::mkNewMetaFile(DirInode& dir, MkFileDetails* mkDetails,
   std::unique_ptr<StripePattern> stripePattern, EntryInfo* outEntryInfo,
   FileInodeStoreData* outInodeData)
{
   UniqueRWLock metaLock(rwlock, SafeRWLock_READ);
   UniqueRWLock dirLock(dir.rwlock, SafeRWLock_WRITE);

   const char* logContext = "Make New Meta File";
   App* app = Program::getApp();
   Config* config = app->getConfig();
   Node& localNode = app->getLocalNode();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

   const std::string& newEntryID = mkDetails->newEntryID.empty() ?
      StorageTk::generateFileID(localNode.getNumID() ) : mkDetails->newEntryID;

   const std::string& parentEntryID = dir.getID();

   NumNodeID ownerNodeID = dir.getIsBuddyMirrored() ?
      NumNodeID(metaBuddyGroupMapper->getLocalGroupID() ) : localNode.getNumID();

   // refuse to create a directory before we even touch the parent. a client could send a request
   // to create an S_IFDIR inode via mkfile.
   if (S_ISDIR(mkDetails->mode))
      return FhgfsOpsErr_INVAL;

   // load DirInode on demand if required, we need it now
   if (!dir.loadIfNotLoadedUnlocked())
      return FhgfsOpsErr_PATHNOTEXISTS;

   CharVector aclXAttr;
   bool needsACL;
   if (config->getStoreClientACLs())
   {
      // Find out if parent dir has an ACL.
      FhgfsOpsErr aclXAttrRes;

      std::tie(aclXAttrRes, aclXAttr, std::ignore) = dir.getXAttr(nullptr,
            PosixACL::defaultACLXAttrName, XATTR_SIZE_MAX);

      if (aclXAttrRes == FhgfsOpsErr_SUCCESS)
      {
         // dir has a default acl.
         PosixACL defaultACL;
         if (!defaultACL.deserializeXAttr(aclXAttr))
         {
            LogContext(logContext).log(Log_ERR, "Error deserializing directory default ACL.");
            return FhgfsOpsErr_INTERNAL;
         }
         else
         {
            if (!defaultACL.empty())
            {
               // Note: This modifies mkDetails->mode as well as the ACL.
               FhgfsOpsErr modeRes = defaultACL.modifyModeBits(mkDetails->mode, needsACL);

               if (modeRes != FhgfsOpsErr_SUCCESS)
                  return modeRes;

               if (needsACL)
                  defaultACL.serializeXAttr(aclXAttr);
            }
            else
            {
               mkDetails->mode &= ~mkDetails->umask;
               needsACL = false;
            }
         }
      }
      else if (aclXAttrRes == FhgfsOpsErr_NODATA)
      {
         // Directory does not have a default ACL - subtract umask from mode bits.
         mkDetails->mode &= ~mkDetails->umask;
         needsACL = false;
      }
      else
      {
         LogContext(logContext).log(Log_ERR, "Error loading directory default ACL.");
         return FhgfsOpsErr_INTERNAL;
      }
   }
   else
   {
      needsACL = false;
   }

   DirEntryType entryType = MetadataTk::posixFileTypeToDirEntryType(mkDetails->mode);
   StatData statData(mkDetails->mode, mkDetails->userID, mkDetails->groupID,
      stripePattern->getAssignedNumTargets(), mkDetails->createTime);

   unsigned origParentUID = dir.getUserIDUnlocked(); // new file, we use the parent UID

   unsigned fileInodeFlags;
   if (dir.getIsBuddyMirrored())
      fileInodeFlags = FILEINODE_FEATURE_BUDDYMIRRORED;
   else
      fileInodeFlags = 0;

   // (note: inodeMetaData constructor clones stripePattern)
   FileInodeStoreData inodeMetaData(newEntryID, &statData, stripePattern.get(), fileInodeFlags,
      origParentUID, parentEntryID, FileInodeOrigFeature_TRUE);

   DirEntry newDentry(entryType, mkDetails->newName, newEntryID, ownerNodeID);

   if (dir.getIsBuddyMirrored())
      newDentry.setBuddyMirrorFeatureFlag(); // buddy mirroring is inherited

   newDentry.setFileInodeData(inodeMetaData);
   inodeMetaData.setPattern(NULL); /* cloned pattern now belongs to newDentry, so make sure it won't
                                      be deleted in inodeMetaData destructor */

   // create a dir-entry with inlined inodes
   FhgfsOpsErr makeRes = dir.makeDirEntryUnlocked(&newDentry);

   if(makeRes == FhgfsOpsErr_SUCCESS)
   { // new entry successfully created
      if (outInodeData)
      { // set caller's outInodeData
         outInodeData->setInodeStatData(statData);
         outInodeData->setPattern(stripePattern.release()); // (will be deleted with outInodeData)
         outInodeData->setEntryID(newEntryID);
      }

   }

   unsigned entryInfoFlags = ENTRYINFO_FEATURE_INLINED;
   if (dir.getIsBuddyMirrored())
      entryInfoFlags |= ENTRYINFO_FEATURE_BUDDYMIRRORED;

   EntryInfo newEntryInfo(ownerNodeID, parentEntryID, newEntryID, mkDetails->newName, entryType,
         entryInfoFlags);

   // apply access ACL calculated from default ACL
   if (needsACL)
   {
      FhgfsOpsErr setXAttrRes = dir.setXAttr(&newEntryInfo, PosixACL::accessACLXAttrName, aclXAttr,
            0, false);

      if (setXAttrRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).log(Log_ERR, "Error setting file ACL.");
         makeRes = FhgfsOpsErr_INTERNAL;
      }
   }

   if (outEntryInfo)
      *outEntryInfo = newEntryInfo;

   return makeRes;
}


FhgfsOpsErr MetaStore::makeDirInode(DirInode& inode)
{
   return inode.storePersistentMetaData();
}

FhgfsOpsErr MetaStore::makeDirInode(DirInode& inode, const CharVector& defaultACLXAttr,
   const CharVector& accessACLXAttr)
{
   return inode.storePersistentMetaData(defaultACLXAttr, accessACLXAttr);
}


FhgfsOpsErr MetaStore::removeDirInode(const std::string& entryID, bool isBuddyMirrored)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   return dirStore.removeDirInode(entryID, isBuddyMirrored);
}

/**
 * Unlink a non-inlined file inode
 *
 * Note: Specialy case without an entryInfo, for fsck only!
 */
FhgfsOpsErr MetaStore::fsckUnlinkFileInode(const std::string& entryID, bool isBuddyMirrored)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   // generic code needs an entryInfo, but most values can be empty for non-inlined inodes
   NumNodeID ownerNodeID;
   std::string parentEntryID;
   std::string fileName;
   DirEntryType entryType = DirEntryType_REGULARFILE;
   int flags = 0;

   EntryInfo entryInfo(ownerNodeID, parentEntryID, entryID, fileName, entryType, flags);

   if (isBuddyMirrored)
      entryInfo.setBuddyMirroredFlag(true);

   return this->fileStore.unlinkFileInode(&entryInfo, NULL);
}

/**
 * @param subDir may be NULL and then needs to be referenced
 */
FhgfsOpsErr MetaStore::unlinkInodeUnlocked(EntryInfo* entryInfo, DirInode* subDir,
   std::unique_ptr<FileInode>* outInode)
{
   if (this->fileStore.isInStore(entryInfo->getEntryID()))
      return fileStore.unlinkFileInode(entryInfo, outInode);

   if (subDir)
      return subDir->fileStore.unlinkFileInode(entryInfo, outInode);

   // not in global /store/map, now per directory

   // Note: We assume, that if the file is buddy mirrored, the parent is mirrored, too
   subDir = referenceDirUnlocked(entryInfo->getParentEntryID(),
      entryInfo->getIsBuddyMirrored(), false);
   if (!subDir)
      return FhgfsOpsErr_PATHNOTEXISTS;

   UniqueRWLock subDirLock(subDir->rwlock, SafeRWLock_READ);

   FhgfsOpsErr unlinkRes = subDir->fileStore.unlinkFileInode(entryInfo, outInode);

   // we can release the DirInode here, as the FileInode is not supposed to be in the
   // DirInodes FileStore anymore
   subDirLock.unlock();
   releaseDirUnlocked(entryInfo->getParentEntryID());

   return unlinkRes;
}


/**
 * @param outFile will be set to the unlinked file and the object must then be deleted by the caller
 * (can be NULL if the caller is not interested in the file)
 */
FhgfsOpsErr MetaStore::unlinkInode(EntryInfo* entryInfo, std::unique_ptr<FileInode>* outInode)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   return unlinkInodeUnlocked(entryInfo, NULL, outInode);
}

/**
 * need the following locks:
 *   this->rwlock: SafeRWLock_WRITE
 *   subDir: reference
 *   subdir->rwlock: SafeRWLock_WRITE
 *
 *   note: caller needs to delete storage chunk files. E.g. via MsgHelperUnlink::unlinkLocalFile()
 */
FhgfsOpsErr MetaStore::unlinkFileUnlocked(DirInode& subdir, const std::string& fileName,
   std::unique_ptr<FileInode>* outInode, EntryInfo* outEntryInfo, bool& outWasInlined)
{
   FhgfsOpsErr retVal;

   std::unique_ptr<DirEntry> dirEntry(subdir.dirEntryCreateFromFileUnlocked(fileName));
   if (!dirEntry)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // dirEntry exists time to make sure we have loaded subdir
   bool loadRes = subdir.loadIfNotLoadedUnlocked();
   if (unlikely(!loadRes) )
      return FhgfsOpsErr_INTERNAL; // if dirEntry exists, subDir also has to exist!

   // set the outEntryInfo
   int additionalFlags = 0;
   std::string parentEntryID = subdir.getID();
   dirEntry->getEntryInfo(parentEntryID, additionalFlags, outEntryInfo);

   if (dirEntry->getIsInodeInlined() )
   {  // inode is inlined into the dir-entry
      retVal = unlinkDirEntryWithInlinedInodeUnlocked(fileName, subdir, dirEntry.get(),
        DirEntry_UNLINK_ID_AND_FILENAME, outInode);

      outWasInlined = true;
   }
   else
   {  // inode and dir-entry are separated fileStore
      retVal = unlinkDentryAndInodeUnlocked(fileName, subdir, dirEntry.get(),
         DirEntry_UNLINK_ID_AND_FILENAME, outInode);

      outWasInlined = false;
   }

   return retVal;
}

/**
 * Unlinks the entire file, so dir-entry and inode.
 *
 * @param fileName friendly name
 * @param outEntryInfo contains the entryInfo of the unlinked file
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
FhgfsOpsErr MetaStore::unlinkFile(DirInode& dir, const std::string& fileName,
   EntryInfo* outEntryInfo, std::unique_ptr<FileInode>* outInode)
{
   const char* logContext = "Unlink File";
   FhgfsOpsErr retVal = FhgfsOpsErr_PATHNOTEXISTS;

   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   UniqueRWLock subDirLock(dir.rwlock, SafeRWLock_WRITE);

   bool wasInlined;
   retVal = unlinkFileUnlocked(dir, fileName, outInode, outEntryInfo, wasInlined);

   subDirLock.unlock();

   /* Give up the read-lock here, unlinkInodeLater() will aquire a write lock. We already did
    * most of the work, just possible back linking of the inode to the disposal dir is missing.
    * As our important work is done, we can also risk to give up the read lock. */
   lock.unlock(); // U N L O C K

   if (retVal != FhgfsOpsErr_INUSE)
      return retVal;

   if (dir.getID() != META_DISPOSALDIR_ID_STR && dir.getID() != META_MIRRORDISPOSALDIR_ID_STR)
   {  // we already successfully deleted the dentry, so all fine for the user
      retVal = FhgfsOpsErr_SUCCESS;
   }

   FhgfsOpsErr laterRes = unlinkInodeLater(outEntryInfo, wasInlined);
   if (laterRes == FhgfsOpsErr_AGAIN)
   {
      /* So the inode was not referenced in memory anymore and probably close() already deleted
       * it. Just make sure here it really does not exist anymore */
      FhgfsOpsErr inodeUnlinkRes = unlinkInode(outEntryInfo, outInode);

      if (unlikely((inodeUnlinkRes != FhgfsOpsErr_PATHNOTEXISTS) &&
         (inodeUnlinkRes != FhgfsOpsErr_SUCCESS) ) )
      {
         LogContext(logContext).logErr(std::string("Failed to unlink inode. Error: ") +
            boost::lexical_cast<std::string>(inodeUnlinkRes));

         retVal = inodeUnlinkRes;
      }
   }

   return retVal;
}

/**
 *
 * Decrement nlink count and remove file inode if link count reaches zero
 */
FhgfsOpsErr MetaStore::unlinkFileInode(EntryInfo* delFileInfo, std::unique_ptr<FileInode>* outInode)
{
   const char* logContext = "Unlink File Inode";
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   FhgfsOpsErr retVal;
   FhgfsOpsErr isUnlinkable = this->fileStore.isUnlinkable(delFileInfo);

   if (isUnlinkable != FhgfsOpsErr_INUSE && isUnlinkable != FhgfsOpsErr_SUCCESS)
      return isUnlinkable;

   if (isUnlinkable == FhgfsOpsErr_INUSE)
   {
      FileInode* inode = this->fileStore.referenceLoadedFile(delFileInfo->getEntryID());
      if (unlikely(!inode))
      {
         LogContext(logContext).logErr("Busy/Inuse file inode found but failed to reference it."
            " EntryID: " + delFileInfo->getEntryID());
         return FhgfsOpsErr_INTERNAL;
      }

      unsigned numHardLinks = inode->getNumHardlinks();

      if (numHardLinks < 2)
      {
         retVal = FhgfsOpsErr_INUSE;
      }
      else
      {
         retVal = FhgfsOpsErr_SUCCESS;
      }

      FhgfsOpsErr decRes = this->fileStore.decLinkCount(*inode, delFileInfo);
      if (decRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).logErr("Failed to decrease the link count. entryID: "
            + delFileInfo->getEntryID());
      }

      this->fileStore.releaseFileInode(inode);
   }
   else
   {
      // File is not IN_USE so decrement link count and if link count reach zero then
      // delete inode file from disk
      MetaFileHandle inode = referenceFileUnlocked(delFileInfo);
      if (unlikely(!inode))
         return FhgfsOpsErr_PATHNOTEXISTS;

      unsigned numHardLinks = inode->getNumHardlinks();
      if (numHardLinks > 1)
      {
         FhgfsOpsErr decRes = this->fileStore.decLinkCount(*inode, delFileInfo);
         if (decRes != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).logErr("Failed to decrease the link count. entryID: "
               + delFileInfo->getEntryID());
         }

         releaseFileUnlocked(delFileInfo->getParentEntryID(), inode);
         retVal = FhgfsOpsErr_SUCCESS;
      }
      else
      {
         // release inode reference before calling unlinkFileInode()
         // because it checks for IN_USE situation
         releaseFileUnlocked(delFileInfo->getParentEntryID(), inode);
         retVal = this->fileStore.unlinkFileInode(delFileInfo, outInode);
      }
   }

   if (retVal != FhgfsOpsErr_INUSE)
      return retVal;

   lock.unlock(); // U N L O C K

   const std::string& parentEntryID = delFileInfo->getParentEntryID();
   if (parentEntryID != META_DISPOSALDIR_ID_STR && parentEntryID != META_MIRRORDISPOSALDIR_ID_STR)
   {
      retVal = FhgfsOpsErr_SUCCESS;
   }

   FhgfsOpsErr laterRes = unlinkInodeLater(delFileInfo, false);
   if (laterRes == FhgfsOpsErr_AGAIN)
   {
      // so the inode was not referenced in memory anymore and probably close() already
      // deleted it. Just make sure here that it really doesn't exists anymore.
      FhgfsOpsErr inodeUnlinkRes = unlinkInode(delFileInfo, outInode);

      if (unlikely((inodeUnlinkRes != FhgfsOpsErr_PATHNOTEXISTS) &&
         (inodeUnlinkRes != FhgfsOpsErr_SUCCESS) ) )
      {
         LogContext(logContext).logErr(std::string("Failed to unlink inode. Error: ") +
            boost::lexical_cast<std::string>(inodeUnlinkRes));

         retVal = inodeUnlinkRes;
      }
   }

   return retVal;
}

/**
 * Unlink a dirEntry with an inlined inode
 */
FhgfsOpsErr MetaStore::unlinkDirEntryWithInlinedInodeUnlocked(const std::string& entryName,
      DirInode& subDir, DirEntry* dirEntry, unsigned unlinkTypeFlags,
      std::unique_ptr<FileInode>* outInode)
{
   const char* logContext = "Unlink DirEntry with inlined inode";

   if (outInode)
      outInode->reset();

   std::string parentEntryID = subDir.getID();

   // when we are here, we no the inode is inlined into the dirEntry
   int flags = ENTRYINFO_FEATURE_INLINED;

   EntryInfo entryInfo;
   dirEntry->getEntryInfo(parentEntryID, flags, &entryInfo);

   FhgfsOpsErr isUnlinkable = isFileUnlinkable(subDir, &entryInfo);

   // note: FhgfsOpsErr_PATHNOTEXISTS cannot happen with isUnlinkable(id, false)

   if (isUnlinkable != FhgfsOpsErr_INUSE && isUnlinkable != FhgfsOpsErr_SUCCESS)
      return isUnlinkable;

   if (isUnlinkable == FhgfsOpsErr_INUSE)
   {
      FhgfsOpsErr retVal;
      // for some reasons we cannot unlink the inode, probably the file is opened. De-inline it.

      // *outInode stays NULL - the caller must not do anything with this inode

      MetaFileHandle inode = referenceLoadedFileUnlocked(subDir, dirEntry->getEntryID() );
      if (!inode)
      {
         LogContext(logContext).logErr("Bug: Busy inode found, but failed to reference it."
            "FileName: " + entryName + ", EntryID: " + dirEntry->getEntryID());
         return FhgfsOpsErr_INTERNAL;
      }

      unsigned numHardlinks = inode->getNumHardlinks();

      if (numHardlinks > 1)
         unlinkTypeFlags &= ~DirEntry_UNLINK_ID;

      bool unlinkError = subDir.unlinkBusyFileUnlocked(entryName, dirEntry, unlinkTypeFlags);
      if (unlinkError)
         retVal = FhgfsOpsErr_INTERNAL;
      else
      {  // unlink success
         if (numHardlinks < 2)
         {
            retVal = FhgfsOpsErr_INUSE;

            // The inode is not inlined anymore, update the in-memory objects

            dirEntry->unsetInodeInlined();
            inode->setIsInlined(false);
         }
         else
         {  // hard link exists, the inode is still inlined
            retVal = FhgfsOpsErr_SUCCESS;
         }

         /* Decrease the link count, but as dentry and inode are still hard linked objects,
          * it also unsets the DENTRY_FEATURE_INODE_INLINE on disk */
         FhgfsOpsErr decRes = subDir.fileStore.decLinkCount(*inode, &entryInfo);
         if (decRes != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).logErr("Failed to decrease the link count!"
               " parentID: "      + entryInfo.getParentEntryID() +
               " entryID: "       + entryInfo.getEntryID()       +
               " fileName: "      + entryName);
         }
      }

      releaseFileUnlocked(subDir, inode);

      return retVal;

      /* We are done here, but the file still might be referenced in the per-dir file store.
       * However, we cannot move it, as we do not have a MetaStore write-lock, but only a
       * read-lock. Therefore that has to be done later on, once we have given up the read-lock. */
   }

   // here, unlinkRes == SUCCESS.
   // dir-entry and inode are inlined. The file is also not opened anymore, so delete it.

   if (!(unlinkTypeFlags & DirEntry_UNLINK_ID))
   {
      // only the dentry-by-name is left, so no need to care about the inode
      return subDir.unlinkDirEntryUnlocked(entryName, dirEntry, unlinkTypeFlags);
   }

   MetaFileHandle inode = referenceFileUnlocked(subDir, &entryInfo);
   if (!inode)
      return FhgfsOpsErr_PATHNOTEXISTS;

   unsigned numHardlinks = inode->getNumHardlinks();

   if (numHardlinks > 1)
      unlinkTypeFlags &= ~DirEntry_UNLINK_ID;

   FhgfsOpsErr retVal = subDir.unlinkDirEntryUnlocked(entryName, dirEntry,
         unlinkTypeFlags);

   if (retVal == FhgfsOpsErr_SUCCESS && numHardlinks > 1)
   {
      FhgfsOpsErr decRes = subDir.fileStore.decLinkCount(*inode, &entryInfo);
      if (decRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).logErr("Failed to decrease the link count!"
            " parentID: "      + entryInfo.getParentEntryID() +
            " entryID: "       + entryInfo.getEntryID()       +
            " entryNameName: " + entryName);
      }
   }

   if (outInode && numHardlinks < 2)
   {
      inode->setIsInlined(false); // last dirEntry gone, so not inlined anymore
      outInode->reset(inode->clone());
   }

   releaseFileUnlocked(subDir, inode);

   return retVal;
}

/**
 * Unlink seperated dirEntry and Inode
 */
FhgfsOpsErr MetaStore::unlinkDentryAndInodeUnlocked(const std::string& fileName,
      DirInode& subdir, DirEntry* dirEntry, unsigned unlinkTypeFlags,
      std::unique_ptr<FileInode>* outInode)
{
   const char* logContext = "Unlink DirEntry with non-inlined inode";

   // unlink dirEntry first
   FhgfsOpsErr retVal = subdir.unlinkDirEntryUnlocked(fileName, dirEntry, unlinkTypeFlags);

   if (retVal != FhgfsOpsErr_SUCCESS)
      return retVal;

   // directory-entry was removed => unlink inode

   // if we are here we know the dir-entry does not inline the inode

   int addionalEntryInfoFlags = 0;
   std::string parentEntryID = subdir.getID();
   EntryInfo entryInfo;

   dirEntry->getEntryInfo(parentEntryID, addionalEntryInfoFlags, &entryInfo);

   FhgfsOpsErr isUnlinkable = isFileUnlinkable(subdir, &entryInfo);
   if (isUnlinkable != FhgfsOpsErr_INUSE && isUnlinkable != FhgfsOpsErr_SUCCESS)
      return isUnlinkable;

   if (isUnlinkable == FhgfsOpsErr_INUSE)
   {
      MetaFileHandle inode = referenceLoadedFileUnlocked(subdir, dirEntry->getEntryID());
      if (unlikely(!inode))
      {
         LogContext(logContext).logErr("Busy/Inuse file inode found but failed to reference it."
            "FileName: " + fileName + ", EntryID: " + dirEntry->getEntryID());
         return FhgfsOpsErr_INTERNAL;
      }

      unsigned numHardLinks = inode->getNumHardlinks();
      if (numHardLinks > 1)
      {
         retVal = FhgfsOpsErr_SUCCESS;
      }
      else
      {
         retVal = FhgfsOpsErr_INUSE;
      }

      FhgfsOpsErr decRes = subdir.fileStore.decLinkCount(*inode, &entryInfo);
      if (decRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).logErr("Failed to decrease the link count."
            " parentEntryID: " + parentEntryID +
            ", entryID: "      + entryInfo.getEntryID() +
            ", entryName: "    + fileName);
      }

      releaseFileUnlocked(subdir, inode);
      return retVal;
   }
   else
   {
      MetaFileHandle inode = referenceFileUnlocked(subdir, &entryInfo);
      if (unlikely(!inode))
         return FhgfsOpsErr_PATHNOTEXISTS;

      unsigned numHardLinks = inode->getNumHardlinks();
      if (numHardLinks > 1)
      {
         FhgfsOpsErr decRes = subdir.fileStore.decLinkCount(*inode, &entryInfo);
         if (decRes != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).logErr("Failed to decrease the link count."
               " parentEntryID: " + parentEntryID +
               ", entryID: "      + entryInfo.getEntryID() +
               ", entryName: "    + fileName);
         }

         releaseFileUnlocked(subdir, inode);
         return FhgfsOpsErr_SUCCESS;
      }
      else
      {
         releaseFileUnlocked(subdir, inode);
         return unlinkInodeUnlocked(&entryInfo, &subdir, outInode);
      }
   }
}

/**
 * Adds the entry to the disposal directory for later (asynchronous) disposal.
 */
FhgfsOpsErr MetaStore::unlinkInodeLater(EntryInfo* entryInfo, bool wasInlined)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);
   return unlinkInodeLaterUnlocked(entryInfo, wasInlined);
}

/**
 * Adds the inode (with a new dirEntry) to the disposal directory for later
 * (on-close or asynchronous) disposal.
 *
 * Note: We are going to set outInode if we determine that the file was closed in the mean time
 *       and all references to this inode shall be deleted.
 */
FhgfsOpsErr MetaStore::unlinkInodeLaterUnlocked(EntryInfo* entryInfo, bool wasInlined)
{
   // Note: We must not try to unlink the inode here immediately, because then the local versions
   //       of the data-object (on the storage nodes) would never be deleted.

   App* app = Program::getApp();
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   const std::string& parentEntryID = entryInfo->getParentEntryID();
   const std::string& entryID = entryInfo->getEntryID();
   bool isBuddyMirrored = entryInfo->getIsBuddyMirrored();

   DirInode* disposalDir = isBuddyMirrored
     ? app->getBuddyMirrorDisposalDir()
     : app->getDisposalDir();
   const std::string& disposalID = disposalDir->getID();

   /* This requires a MetaStore write lock, therefore only can be done here and not in common
    * unlink code, as the common unlink code only has a MetaStore read-lock. */
   if (!this->fileStore.isInStore(entryID) )
   {
      // Note: we assume that if the inode is mirrored, the parent is mirrored, too
      bool moveRes = moveReferenceToMetaFileStoreUnlocked(parentEntryID, isBuddyMirrored, entryID);
      if (!moveRes)
         return FhgfsOpsErr_INTERNAL; /* a critical error happened, better don't do
                                       * anything with this inode anymore */
   }

   entryInfo->setParentEntryID(disposalID); // update the dirID to disposalDir

   // set inode nlink count to 0.
   // we assume the inode is typically already referenced by someone (otherwise we wouldn't need to
   // unlink later) and this allows for faster checking on-close than the disposal dir check.

   // do not load the inode from disk first.
   FileInode* inode = this->fileStore.referenceLoadedFile(entryID);
   if (!inode)
   {
      /* If we cannot reference the inode from memory, we raced with close().
       * This is possible as we gave up all locks in unlinkFile() and it means the inode/file shall
       * be deleted entirely on disk now. */

      entryInfo->setInodeInlinedFlag(false);
      return FhgfsOpsErr_AGAIN;
   }

   int linkCount = inode->getNumHardlinks();;

   this->fileStore.releaseFileInode(inode);

   /* Now link to the disposal-dir if required. If the inode was inlined into the dentry,
    * the dentry/inode unlink code already links to the disposal dir and we do not need to do
    * this work. */
   if (!wasInlined && linkCount == 0)
   {
      const std::string& inodePath  = MetaStorageTk::getMetaInodePath(
            isBuddyMirrored
               ? app->getBuddyMirrorInodesPath()->str()
               : app->getInodesPath()->str(),
            entryID);

      /* NOTE: If we are going to have another inode-format, than the current
       *       inode-inlined into the dentry, we need to add code for that here. */

      disposalDir->linkFileInodeToDir(inodePath, entryID); // use entryID as file name
      // ignore the return code here, as we cannot do anything about it anyway.
   }

   return retVal;
}

/**
 * Reads all inodes from the given inodes storage hash subdir.
 *
 * Note: This is intended for use by Fsck only.
 *
 * Note: Offset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
 * Note: You have reached the end of the directory when
 *       "outDirInodes->size() + outFileInodes->size() != maxOutInodes".
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
   int64_t* outNewOffset, bool isBuddyMirrored)
{
   const char* logContext = "MetaStore (get all inodes inc)";

   App* app = Program::getApp();
   MirrorBuddyGroupMapper* bgm = app->getMetaBuddyGroupMapper();

   if (isBuddyMirrored &&
         (bgm->getLocalBuddyGroup().secondTargetID == app->getLocalNode().getNumID().val()
          || bgm->getLocalGroupID() == 0))
      return FhgfsOpsErr_SUCCESS;

   NumNodeID rootNodeNumID = app->getMetaRoot().getID();
   NumNodeID localNodeNumID = isBuddyMirrored
      ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID())
      : app->getLocalNode().getNumID();

   StringList entryIDs;
   unsigned firstLevelHashDir;
   unsigned secondLevelHashDir;
   StorageTk::splitHashDirs(hashDirNum, &firstLevelHashDir, &secondLevelHashDir);

   FhgfsOpsErr readRes = getAllEntryIDFilesIncremental(firstLevelHashDir, secondLevelHashDir,
      lastOffset, maxOutInodes, &entryIDs, outNewOffset, isBuddyMirrored);

   if (readRes != FhgfsOpsErr_SUCCESS)
   {
      LogContext(logContext).logErr(
         "Failed to read inodes from hash dirs; "
         "HashDir Level 1: " + StringTk::uintToStr(firstLevelHashDir) + "; "
         "HashDir Level 2: " + StringTk::uintToStr(firstLevelHashDir) );
      return readRes;
   }

   // the actual entry processing
   for ( StringListIter entryIDIter = entryIDs.begin(); entryIDIter != entryIDs.end();
      entryIDIter++ )
   {
      const std::string& entryID = *entryIDIter;

      // now try to reference the file and see what we got
      MetaFileHandle fileInode;
      DirInode* dirInode = NULL;

      referenceInode(entryID, isBuddyMirrored, fileInode, dirInode);

      if (dirInode)
      {
         // entry is a directory
         std::string parentDirID;
         NumNodeID parentNodeID;
         dirInode->getParentInfo(&parentDirID, &parentNodeID);

         NumNodeID ownerNodeID = dirInode->getOwnerNodeID();

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
            dirInode->getStripePattern(), NULL, &stripeTargets);

         FsckDirInode fsckDirInode(entryID, parentDirID, parentNodeID, ownerNodeID,
            statData.getFileSize(), statData.getNumHardlinks(), stripeTargets,
            stripePatternType, localNodeNumID, isBuddyMirrored, true,
            dirInode->getIsBuddyMirrored() != isBuddyMirrored);

         outDirInodes->push_back(fsckDirInode);

         this->releaseDir(entryID);
      }
      else
      if (fileInode)
      {
         // directory not successful => must be a file-like object

         // create a FsckFileInode with data
         std::string parentDirID;
         NumNodeID parentNodeID;
         UInt16Vector stripeTargets;

         PathInfo pathInfo;

         unsigned userID;
         unsigned groupID;

         int64_t fileSize;
         unsigned numHardLinks;
         uint64_t numBlocks;

         StatData statData;

         fileInode->getPathInfo(&pathInfo);

         FhgfsOpsErr statRes = fileInode->getStatData(statData);

         if ( statRes == FhgfsOpsErr_SUCCESS )
         {
            userID = statData.getUserID();
            groupID = statData.getGroupID();
            fileSize = statData.getFileSize();
            numHardLinks = statData.getNumHardlinks();
            numBlocks = statData.getNumBlocks();
         }
         else
         { // couldn't get the stat data
            LogContext(logContext).logErr(std::string("Unable to stat file inode: ") +
               entryID + std::string(". SysErr: ") + boost::lexical_cast<std::string>(statRes));

            userID = 0;
            groupID = 0;
            fileSize = 0;
            numHardLinks = 0;
            numBlocks = 0;
         }

         StripePattern* stripePattern = fileInode->getStripePattern();
         unsigned chunkSize;
         FsckStripePatternType stripePatternType = FsckTk::stripePatternToFsckStripePattern(
            stripePattern, &chunkSize, &stripeTargets);

         FsckFileInode fsckFileInode(entryID, parentDirID, parentNodeID, pathInfo, userID, groupID,
               fileSize, numHardLinks, numBlocks, stripeTargets, stripePatternType, chunkSize,
               localNodeNumID, 0, 0, false, isBuddyMirrored, true,
               fileInode->getIsBuddyMirrored() != isBuddyMirrored);

         outFileInodes->push_back(fsckFileInode);

         // parentID is absolutely irrelevant here, because we know that this inode is not inlined
         this->releaseFile("", fileInode);
      }
      else
      { // something went wrong with inode loading

         // create a dir inode as dummy
         const UInt16Vector stripeTargets;
         const FsckDirInode fsckDirInode(entryID, "", NumNodeID(), NumNodeID(), 0, 0, stripeTargets,
            FsckStripePatternType_INVALID, localNodeNumID, isBuddyMirrored, false, false);

         outDirInodes->push_back(fsckDirInode);
      }

   } // end of for loop

   return FhgfsOpsErr_SUCCESS;
}


/**
 * Reads all raw entryID filenames from the given "inodes" storage hash subdirs.
 *
 * Note: This is intended for use by Fsck.
 *
 * Note: Offset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
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
   StringList* outEntryIDFiles, int64_t* outNewOffset, bool buddyMirrored)
{
   const char* logContext = "Inode (get entry files inc)";
   App* app = Program::getApp();

   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   const std::string inodesPath =
      buddyMirrored ? app->getBuddyMirrorInodesPath()->str()
                    : app->getInodesPath()->str();

   const std::string path = StorageTkEx::getMetaInodeHashDir(
      inodesPath, firstLevelhashDirNum, secondLevelhashDirNum);


   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   DIR* dirHandle = opendir(path.c_str() );
   if(!dirHandle)
   {
      LogContext(logContext).logErr(std::string("Unable to open entries directory: ") +
         path + ". SysErr: " + System::getErrString() );

      return FhgfsOpsErr_INTERNAL;
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

   return retVal;
}

void MetaStore::getReferenceStats(size_t* numReferencedDirs, size_t* numReferencedFiles)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);

   *numReferencedDirs = dirStore.getSize();
   *numReferencedFiles = fileStore.getSize();
}

void MetaStore::getCacheStats(size_t* numCachedDirs)
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   *numCachedDirs = dirStore.getCacheSize();
}

/**
 * Asynchronous cache sweep.
 *
 * @return true if a cache flush was triggered, false otherwise
 */
bool MetaStore::cacheSweepAsync()
{
   UniqueRWLock lock(rwlock, SafeRWLock_READ);
   return dirStore.cacheSweepAsync();
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
   App* app = Program::getApp();

   DirInode* disposalDir = inode->getIsBuddyMirrored()
      ? Program::getApp()->getBuddyMirrorDisposalDir()
      : Program::getApp()->getDisposalDir();

   UniqueRWLock metaLock(this->rwlock, SafeRWLock_READ);
   UniqueRWLock dirLock(disposalDir->rwlock, SafeRWLock_WRITE);

   const std::string& fileName = inode->getEntryID(); // ID is also the file name
   DirEntryType entryType = MetadataTk::posixFileTypeToDirEntryType(inode->getMode() );

   EntryInfo entryInfo;
   NumNodeID ownerNodeID = inode->getIsBuddyMirrored() ?
         NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() ) : app->getLocalNode().getNumID();
   entryInfo.set(ownerNodeID, "", inode->getEntryID(), "", entryType, 0);

   FhgfsOpsErr retVal = mkMetaFileUnlocked(*disposalDir, fileName, &entryInfo, inode);
   if (retVal != FhgfsOpsErr_SUCCESS)
   {
      log.log(Log_WARNING,
         std::string("Failed to create disposal file for id: ") + fileName + "; "
         "Storage chunks will not be entirely deleted!");
   }

   return retVal;
}

/**
 * Retrieves entry data for a given file or directory.
 *
 * @param dirInode Pointer to the parent directory inode.
 * @param entryName Name of the entry to retrieve data for.
 * @param outInfo Pointer to store the retrieved EntryInfo.
 * @param outInodeMetaData Pointer to store inode metadata (may be NULL).
 *
 * @return A pair of:
 *         - FhgfsOpsErr
 *           SUCCESS if the entry was found and is not referenced
 *           DYNAMICATTRIBSOUTDATED if the entry was found but might have outdated attributes
 *           PATHNOTEXISTS if the entry does not exist
 *         - bool: true if entry is a regular file and is currently open, false otherwise
 *
 * Locking: No lock must be taken already.
 */
std::pair<FhgfsOpsErr, bool> MetaStore::getEntryData(DirInode *dirInode, const std::string& entryName,
   EntryInfo* outInfo, FileInodeStoreData* outInodeMetaData)
{
   FhgfsOpsErr retVal = dirInode->getEntryData(entryName, outInfo, outInodeMetaData);

   if (retVal == FhgfsOpsErr_SUCCESS && DirEntryType_ISREGULARFILE(outInfo->getEntryType()))
   {
      /* Hint for the caller not to rely on outInodeMetaData, properly handling close-races
       * is too difficult and in the end probably slower than to just re-get the EAs from the
       * inode (fsIDs/entryID). */
      return {FhgfsOpsErr_DYNAMICATTRIBSOUTDATED, false};
   }

   if (retVal == FhgfsOpsErr_DYNAMICATTRIBSOUTDATED)
      return {retVal, true};  // entry is a regular file and currently open

   return {retVal, false};
}

/**
 * Get inode disk data for non-inlined inode
 *
 * @param outInodeMetaData might be NULL
 * @return FhgfsOpsErr_SUCCESS if inode exists
 *         FhgfsOpsErr_PATHNOTEXISTS if the inode does not exist
 *
 * Locking: No lock must be taken already.
 */
FhgfsOpsErr MetaStore::getEntryData(EntryInfo* inEntryInfo, FileInodeStoreData* outInodeMetaData)
{
   std::unique_ptr<FileInode> inode(FileInode::createFromEntryInfo(inEntryInfo));
   if (unlikely(!inode))
      return FhgfsOpsErr_PATHNOTEXISTS;

   *outInodeMetaData = *(inode->getInodeDiskData());
   inode->getInodeDiskData()->setPattern(NULL);
   return FhgfsOpsErr_SUCCESS;
}

/**
 * Create a hard-link within a directory.
 */
FhgfsOpsErr MetaStore::linkInSameDir(DirInode& parentDir, EntryInfo* fromFileInfo,
   const std::string& fromName, const std::string& toName)
{
   const char* logContext = "link in same dir";
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   MetaFileHandle fromFileInode;

   UniqueRWLock metaLock(rwlock, SafeRWLock_READ);
   UniqueRWLock parentDirLock(parentDir.rwlock, SafeRWLock_WRITE);

   fromFileInode = referenceFileUnlocked(parentDir, fromFileInfo);
   if (!fromFileInode)
   {
      retVal = FhgfsOpsErr_PATHNOTEXISTS;
      goto outUnlock;
   }

   if (!fromFileInode->getIsInlined() )
   {  // not supported
      retVal = FhgfsOpsErr_INTERNAL;
      goto outReleaseInode;
   }

   if (this->fileStore.isInStore(fromFileInfo->getEntryID() ) )
   {  // not supported
      retVal = FhgfsOpsErr_INTERNAL;
      goto outReleaseInode;
   }
   else
   {
      // not in global /store/map, now per directory

      FhgfsOpsErr incRes = parentDir.fileStore.incLinkCount(*fromFileInode, fromFileInfo);
      if (incRes != FhgfsOpsErr_SUCCESS)
      {
         retVal = FhgfsOpsErr_INTERNAL;
         goto outReleaseInode;
      }

      retVal = parentDir.linkFilesInDirUnlocked(fromName, *fromFileInode, toName);
      if (retVal != FhgfsOpsErr_SUCCESS)
      {
         FhgfsOpsErr decRes = parentDir.fileStore.decLinkCount(*fromFileInode, fromFileInfo);
         if (decRes != FhgfsOpsErr_SUCCESS)
            LogContext(logContext).logErr("Warning: Creating the link failed and decreasing "
               "the inode link count again now also failed!"
               " parentDir : " + parentDir.getID()   +
               " entryID: "    + fromFileInfo->getEntryID() +
               " fileName: "   + fromName);
      }
   }

outReleaseInode:
   releaseFileUnlocked(parentDir, fromFileInode);

outUnlock:
   parentDirLock.unlock();

   return retVal;
}

/**
 *  create new hardlink
 *
 *  1. perform inode deinline if its an inlined inode
 *  2. increment link count by 1
 */
FhgfsOpsErr MetaStore::makeNewHardlink(EntryInfo* fromFileInfo)
{
   UniqueRWLock metaLock(rwlock, SafeRWLock_WRITE);
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   bool isInlined = fromFileInfo->getIsInlined();

   // try to load an unreferenced inode from disk
   std::unique_ptr<FileInode> fInode(FileInode::createFromEntryInfo(fromFileInfo));
   if (unlikely(!fInode))
      return FhgfsOpsErr_PATHNOTEXISTS;

   // get value of inlined flag from on-disk metadata
   isInlined = fInode->getIsInlined();

   if (isInlined)
   {
      DirInode* dir = referenceDirUnlocked(fromFileInfo->getParentEntryID(),
         fromFileInfo->getIsBuddyMirrored(), true);

      if (unlikely(!dir))
         return FhgfsOpsErr_PATHNOTEXISTS;

      retVal = verifyAndMoveFileInodeUnlocked(*dir, fromFileInfo, MODE_DEINLINE);
      releaseDirUnlocked(dir->getID());
   }

   if (retVal == FhgfsOpsErr_SUCCESS)
   {
      // move existing inode references from dir specific store to global store
      // to make sure we always use global store for non-inlined file inode(s)
      if (!this->fileStore.isInStore(fromFileInfo->getEntryID()))
      {
         this->moveReferenceToMetaFileStoreUnlocked(fromFileInfo->getParentEntryID(),
            fromFileInfo->getIsBuddyMirrored(), fromFileInfo->getEntryID());
      }

      MetaFileHandle inode = referenceFileUnlocked(fromFileInfo);
      if (unlikely(!inode))
         return FhgfsOpsErr_PATHNOTEXISTS;

      if (inode->getNumHardlinks() >= 1)
      {
         if (!inode->incDecNumHardLinks(fromFileInfo, 1))
            retVal = FhgfsOpsErr_INTERNAL;
         else
            retVal = FhgfsOpsErr_SUCCESS;
      }
      else
         retVal = FhgfsOpsErr_PATHNOTEXISTS;

      releaseFileUnlocked(fromFileInfo->getParentEntryID(), inode);
   }

   return retVal;
}

/**
 * Deinline or reinline file's Inode on current meta-data server. It first verifies Inode
 * location (inlined or deinlined) and then performes requested inode movement if needed
 *
 * @returns FhgfsOpsErr_SUCCESS on success. FhgfsOpsErr_PATHNOTEXISTS if file does not exists
 *          FhgfsOpsErr_INTERNAL on any other error
 *
 * @param parentDir     parent directory's inode object
 * @param fileInfo      entryInfo of file for which inode movement is requested
 * @param moveMode      requested fileInode mode (i.e. deinline or reinline)
 *
 */
FhgfsOpsErr MetaStore::verifyAndMoveFileInode(DirInode& parentDir, EntryInfo* fileInfo, FileInodeMode moveMode)
{
   UniqueRWLock metaLock(rwlock, SafeRWLock_WRITE);
   return verifyAndMoveFileInodeUnlocked(parentDir, fileInfo, moveMode);
}

FhgfsOpsErr MetaStore::verifyAndMoveFileInodeUnlocked(DirInode& parentDir, EntryInfo* fileInfo,
   FileInodeMode moveMode)
{
   App* app = Program::getApp();
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   if (!parentDir.loadIfNotLoadedUnlocked())
   {
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   DirEntry fileDentry(fileInfo->getFileName());
   if (!parentDir.getDentry(fileInfo->getFileName(), fileDentry))
   {
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   if (moveMode == MODE_INVALID)
      return FhgfsOpsErr_INTERNAL;

   bool isInodeMoveRequired = ((moveMode == MODE_DEINLINE) && fileDentry.getIsInodeInlined()) ||
                              ((moveMode == MODE_REINLINE) && !fileDentry.getIsInodeInlined());

   if (isInodeMoveRequired)
   {
      // prepare dentry path
      const Path* dentriesPath =
         fileInfo->getIsBuddyMirrored() ? app->getBuddyMirrorDentriesPath() : app->getDentriesPath();
      std::string dirEntryPath = MetaStorageTk::getMetaDirEntryPath(dentriesPath->str(), parentDir.getID());

      switch (moveMode)
      {
         case MODE_DEINLINE:
         {
            retVal = deinlineFileInode(parentDir, fileInfo, fileDentry, dirEntryPath);
            break;
         }

         case MODE_REINLINE:
         {
            retVal = reinlineFileInode(parentDir, fileInfo, fileDentry, dirEntryPath);
            break;
         }

         case MODE_INVALID:
            break;  // added just to please compiler (already handled above)
      }
   }
   else
   {
      // due to unexpected failures (like node crash) there may some inconsistencies present in
      // filesystem like duplicate inode(s) or dentry-by-entryID file missing for an inlined inode
      // following code takes care to recover from such situations if user re-runs previous failed
      // operation again (i.e. run beegfs-ctl command again in case of error)
      switch (moveMode)
      {
         case MODE_DEINLINE:
         {
            // remove dentry-by-entryID file
            parentDir.unlinkDirEntry(fileInfo->getFileName(), &fileDentry,  DirEntry_UNLINK_ID);
            break;
         }

         case MODE_REINLINE:
         {
            // check if duplicate inode exists in inode Tree and remove associated meta file
            EntryInfo fileInfoCopy(*fileInfo);
            fileInfoCopy.setInodeInlinedFlag(false);
            std::unique_ptr<FileInode> inode(FileInode::createFromEntryInfo(&fileInfoCopy));
            if (likely(inode))
            {
               FileInode::unlinkStoredInodeUnlocked(fileInfo->getEntryID(), fileInfo->getIsBuddyMirrored());
            }
            break;
         }

         case MODE_INVALID:
            break;  // nothing to do
      }
   }

   return retVal;
}

/**
 * Helper function of verifyAndMoveFileInode()
 * Makes a file Inode non-inlined. Caller should check if inode is not already nonInlined
 */
FhgfsOpsErr MetaStore::deinlineFileInode(DirInode& parentDir, EntryInfo* entryInfo, DirEntry& dentry, std::string const& dirEntryPath)
{
   UniqueRWLock dirLock(parentDir.rwlock, SafeRWLock_WRITE);

   MetaFileHandle fileInode = referenceFileUnlocked(parentDir, entryInfo);
   if (!fileInode)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // 1. write inode meta-data file in inode's tree
   fileInode->setIsInlined(false);
   if (!fileInode->updateInodeOnDisk(entryInfo))
   {
      releaseFileUnlocked(parentDir, fileInode);
      return FhgfsOpsErr_INTERNAL;
   }

   // 2. copy all user-defined xattrs from the inlined inode to the non-inlined inode.
   StringVector xAttrNames;
   FhgfsOpsErr listXAttrRes;

   // retrieve the list of xattr names from inlined inode.
   std::tie(listXAttrRes, xAttrNames) = parentDir.listXAttr(entryInfo);

   if (listXAttrRes == FhgfsOpsErr_SUCCESS)
   {
      for (const auto& xAttrName : xAttrNames)
      {
         CharVector xAttrValue;
         FhgfsOpsErr getXAttrRes;

         // retrieve the value of the current xattr using inlined inode.
         std::tie(getXAttrRes, xAttrValue, std::ignore) = parentDir.getXAttr(entryInfo,
            xAttrName, XATTR_SIZE_MAX);

         if (getXAttrRes == FhgfsOpsErr_SUCCESS)
         {
            // set the xattr with its corresponding value to non-inlined inode.
            fileInode->setXAttr(entryInfo, xAttrName, xAttrValue, 0);
         }
      }
   }

   // 3. update feature flags for passed-in dentry object so that when its
   // saved to disk again, it gets written in VER-3 format
   dentry.unsetInodeInlined();
   unsigned flags = dentry.getDentryFeatureFlags();
   flags &= ~(DENTRY_FEATURE_IS_FILEINODE);
   dentry.setDentryFeatureFlags(flags);
   dentry.getInodeStoreData()->setOrigFeature(FileInodeOrigFeature_UNSET);

   // 4. write updated dentry data to disk
   bool saveRes = dentry.storeUpdatedDirEntry(dirEntryPath);

   // 5. unlink dentry-by-entryID file present in '#fSiDs#' directory
   if (saveRes)
   {
      parentDir.unlinkDirEntryUnlocked(entryInfo->getFileName(), &dentry, DirEntry_UNLINK_ID);
   }

   releaseFileUnlocked(parentDir, fileInode);
   return FhgfsOpsErr_SUCCESS;
}

/**
 * Helper function of verifyAndMoveFileInode()
 * Makes a file Inode inlined. Caller should check if inode is already not inlined
 */
FhgfsOpsErr MetaStore::reinlineFileInode(DirInode& parentDir, EntryInfo* entryInfo, DirEntry& dentry, std::string const& dirEntryPath)
{
   const char* logContext = "make fileInode Inlined";
   MetaFileHandle fileInode = referenceFileUnlocked(entryInfo);
   if (!fileInode)
      return FhgfsOpsErr_PATHNOTEXISTS;

   UniqueRWLock dirLock(parentDir.rwlock, SafeRWLock_WRITE);

   // 1. set inode specific data in dentry object after updating inode feature flags
   fileInode->setIsInlined(true);
   dentry.setFileInodeData(*(fileInode->getInodeDiskData()));
   fileInode->getInodeDiskData()->setPattern(NULL);

   // 2. create link in '#fSiDs#' directory for dentry-by-entryID file
   // needed because an inlined inode always have this link present
   std::string idPath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) + entryInfo->getEntryID();
   std::string namePath = dirEntryPath + "/" + entryInfo->getFileName();
   int linkRes = link(namePath.c_str(), idPath.c_str());

   if (linkRes)
   {
      if (errno != EEXIST)
      {
         LogContext(logContext).logErr("Creating dentry-by-entryid file failed: Path: "
            + idPath + " SysErr: " + System::getErrString());

         releaseFileUnlocked(parentDir, fileInode);
         return FhgfsOpsErr_INTERNAL;
      }
   }

   // 3. now save in-memory dentry data (having inlined inode) onto disk
   bool saveRes = dentry.storeUpdatedDirEntry(dirEntryPath);

   // 4. delete non-inlined inode meta file from disk
   if (saveRes)
   {
      if (!FileInode::unlinkStoredInodeUnlocked(entryInfo->getEntryID(), entryInfo->getIsBuddyMirrored()))
      {
         releaseFileUnlocked(parentDir, fileInode);
         return FhgfsOpsErr_INTERNAL;
      }
   }

   releaseFileUnlocked(parentDir, fileInode);
   return FhgfsOpsErr_SUCCESS;
}

/**
 * Check if duplicate inodes exists or not (i.e. both inlined + nonInlined inode)
 * If yes, then remove nonInlined inode to fix duplicacy
 *
 * @param parentDir parant directory of file for which duplicacy of inode needs to be checked
 * @param entryInfo entry info of file
 */
FhgfsOpsErr MetaStore::checkAndRepairDupFileInode(DirInode& parentDir, EntryInfo* entryInfo)
{
   UniqueRWLock metaLock(rwlock, SafeRWLock_WRITE);
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   if (!parentDir.loadIfNotLoadedUnlocked())
   {
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   // first try to load inlined inode
   entryInfo->setInodeInlinedFlag(true);
   MetaFileHandle inlinedInode = referenceFileUnlocked(entryInfo);
   if (!(inlinedInode && inlinedInode->getIsInlined()))
   {
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   UniqueRWLock dirLock(parentDir.rwlock, SafeRWLock_WRITE);

   // now try to load nonInlined inode
   entryInfo->setInodeInlinedFlag(false);
   FileInode*  nonInlinedInode = FileInode::createFromEntryInfo(entryInfo);
   if (!nonInlinedInode)
      return retVal;

   // if we are here then we know that both inlined + nonInlined inodes exists and
   // we can safely remove nonInlined inode to fix duplicacy
   if (!FileInode::unlinkStoredInodeUnlocked(entryInfo->getEntryID(), entryInfo->getIsBuddyMirrored()))
   {
      retVal = FhgfsOpsErr_INTERNAL;
   }

   releaseFileUnlocked(parentDir, inlinedInode);
   SAFE_DELETE(nonInlinedInode);
   return retVal;
}

/**
 * Get the raw contents of the metadata file specified by path.
 * Note: This is intended to be used by the buddy resyncer only.
 *
 * @returns FhgfsOpsErr_SUCCESS on success. FhgfsOpsErr_PATHNOTEXISTS if file does not exist.
 *          FhgfsOpsErr_INTERNAL on any other error.
 * @param contents The file contents will be stored in this vector. The vector will be assigned the
 *        new contens, any old contents will be lost.
 */
FhgfsOpsErr MetaStore::getRawMetadata(const Path& path, CharVector& contents)
{
   App* app = Program::getApp();
   const bool useXAttrs = app->getConfig()->getStoreUseExtendedAttribs();
   const std::string metaPath = app->getMetaPath();

   char buf[META_SERBUF_SIZE];
   ssize_t readRes;

   if (useXAttrs)
   {
      // Load from Xattr
      readRes = ::getxattr(path.str().c_str(), META_XATTR_NAME, buf, META_SERBUF_SIZE);
      if (readRes <= 0)
      {
         if (readRes == -1 && errno == ENOENT)
         {
            LOG(GENERAL, WARNING, "Metadata file does not exist", path);
            return FhgfsOpsErr_PATHNOTEXISTS;
         }

         LOG(GENERAL, WARNING, "Unable to read metadata file", path, sysErr);
         return FhgfsOpsErr_INTERNAL;
      }
   }
   else
   {
      // Load from file contents.

      int fd = open(path.str().c_str(), O_NOATIME | O_RDONLY, 0);
      if (fd == -1)
      {
         if (errno != ENOENT)
         {
            LOG(GENERAL, WARNING, "Unable to read metadata file", path, sysErr);
            return FhgfsOpsErr_INTERNAL;
         }

         LOG(GENERAL, WARNING, "Metadata file does not exist", path);
         return FhgfsOpsErr_PATHNOTEXISTS;
      }

      readRes = ::read(fd, buf, META_SERBUF_SIZE);
      if (readRes <= 0)
      {
         LOG(GENERAL, ERR, "Unable to read metadata file", path, sysErr);
         close(fd);
         return FhgfsOpsErr_INTERNAL;
      }

      close(fd);
   }

   contents.assign(buf, buf + readRes);
   return FhgfsOpsErr_SUCCESS;
}


/**
 * Create a file or directory for metadata resync. The caller is responsible for filling the
 * resulting object with metadata content or xattrs.
 * Note: This is intended to be used by the buddy resynver only.
 *
 * A metatada inode which does not exist yet is created.
 */
std::pair<FhgfsOpsErr, IncompleteInode> MetaStore::beginResyncFor(const Path& path,
   bool isDirectory)
{
   // first try to create the path directly, and if that fails with ENOENT (ie a directory in the
   // path does not exist), create all parent directories and try again.
   for (int round = 0; round < 2; round++)
   {
      int mkRes;

      if (isDirectory)
      {
         mkRes = ::mkdir(path.str().c_str(), 0755);
         if (mkRes == 0 || errno == EEXIST)
            mkRes = ::open(path.str().c_str(), O_DIRECTORY);
      }
      else
         mkRes = ::open(path.str().c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);

      if (mkRes < 0 && errno == ENOENT && round == 0)
      {
         if (!StorageTk::createPathOnDisk(path, true))
         {
            LOG(GENERAL, ERR, "Could not create metadata path", path, isDirectory);
            return {FhgfsOpsErr_INTERNAL, IncompleteInode{}};
         }

         continue;
      }

      if (mkRes < 0)
         break;

      return {FhgfsOpsErr_SUCCESS, IncompleteInode(mkRes)};
   }

   LOG(GENERAL, ERR, "Could not create metadata file/directory", path, isDirectory, sysErr);
   return {FhgfsOpsErrTk::fromSysErr(errno), IncompleteInode{}};
}

/**
 * Deletes a raw metadata file specified by path.
 * Note: This is intended to be used by the buddy resyncer only.
 */
FhgfsOpsErr MetaStore::unlinkRawMetadata(const Path& path)
{
   App* app = Program::getApp();
   const std::string metaPath = app->getMetaPath();

   int unlinkRes = ::unlink(path.str().c_str());

   if (!unlinkRes)
      return FhgfsOpsErr_SUCCESS;

   if (errno == ENOENT)
   {
      LOG(GENERAL, DEBUG, "Metadata file does not exist", path);
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   LOG(GENERAL, DEBUG, "Error unlinking metadata file", path, sysErr);
   return FhgfsOpsErr_INTERNAL;
}

void MetaStore::invalidateMirroredDirInodes()
{
   dirStore.invalidateMirroredDirInodes();
}
