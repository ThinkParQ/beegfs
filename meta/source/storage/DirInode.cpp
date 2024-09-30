#include <net/msghelpers/MsgHelperXAttr.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/Raid10Pattern.h>
#include <common/toolkit/TimeAbs.h>
#include <program/Program.h>
#include <storage/PosixACL.h>
#include <toolkit/XAttrTk.h>

#include <sys/types.h>
#include <sys/xattr.h>
#include <dirent.h>

#include "DiskMetaData.h"
#include "DirEntry.h"
#include "DirInode.h"

#include <boost/lexical_cast.hpp>

/**
 * Constructur used to create new directories.
 */
DirInode::DirInode(const std::string& id, int mode, unsigned userID, unsigned groupID,
   NumNodeID ownerNodeID, const StripePattern& stripePattern, bool isBuddyMirrored)
    : id(id),
      ownerNodeID(ownerNodeID),
      parentNodeID(0), // 0 means undefined
      featureFlags( DIRINODE_FEATURE_EARLY_SUBDIRS | DIRINODE_FEATURE_STATFLAGS |
                     (isBuddyMirrored ? DIRINODE_FEATURE_BUDDYMIRRORED : 0) ),
      exclusive(false),
      numSubdirs(0),
      numFiles(0),
      entries(id, isBuddyMirrored),
      isLoaded(true)
{
   this->stripePattern      = stripePattern.clone();

   int64_t currentTimeSecs  = TimeAbs().getTimeval()->tv_sec;
   SettableFileAttribs settableAttribs = {mode, userID, groupID, currentTimeSecs, currentTimeSecs};
   this->statData.setInitNewDirInode(&settableAttribs, currentTimeSecs);
}

/**
 * Create a stripe pattern based on the current pattern config and add targets to it.
 *
 * @param preferredTargets may be NULL
 * @param numtargets 0 to use setting from directory (-1/~0 for all available targets).
 * @param chunksize 0 to use setting from directory, must be 2^n and >=64Ki otherwise.
 * @param storagePoolId if this is set only targets from this pool are considered and settings of
 *        parent directory are ignored
 * @return NULL on error, new object on success.
 */
StripePattern* DirInode::createFileStripePattern(const UInt16List* preferredTargets,
   unsigned numtargets, unsigned chunksize, StoragePoolId storagePoolId)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   StripePattern* filePattern = createFileStripePatternUnlocked(preferredTargets, numtargets,
      chunksize, storagePoolId);

   safeLock.unlock(); // U N L O C K

   return filePattern;
}

/**
 * @param preferredTargets may be NULL
 * @param numtargets 0 to use setting from directory (-1/~0 for all available targets).
 * @param chunksize 0 to use setting from directory, must be 2^n and >=64Ki otherwise.
 * @param storagePoolId if this is set only targets from this pool are considered and settings of
 *        parent directory are ignored
 * @return NULL on error, new object on success.
 */
StripePattern* DirInode::createFileStripePatternUnlocked(const UInt16List* preferredTargets,
   unsigned numtargets, unsigned chunksize, StoragePoolId storagePoolId)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   TargetChooserType chooserType = cfg->getTuneTargetChooserNum();

   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return NULL;

   if(!chunksize)
      chunksize = stripePattern->getChunkSize(); // use default dir chunksize
   else
   if(unlikely( (chunksize < STRIPEPATTERN_MIN_CHUNKSIZE) ||
                !MathTk::isPowerOfTwo(chunksize) ) )
      return NULL; // invalid chunksize given

   StripePattern* filePattern;

   // choose some available storage targets

   unsigned desiredNumTargets = numtargets ? numtargets : stripePattern->getDefaultNumTargets();
   unsigned minNumRequiredTargets = stripePattern->getMinNumTargets();

   UInt16Vector stripeTargets;

   if (desiredNumTargets < minNumRequiredTargets)
      desiredNumTargets = minNumRequiredTargets;

   // limit maximum number of targets per file to a safe value (such that serialized stripe patterns
   // are bounded in size)
   if(desiredNumTargets > DIRINODE_MAX_STRIPE_TARGETS)
      desiredNumTargets = DIRINODE_MAX_STRIPE_TARGETS;

   // get the storage pool, which is set to later restrict targets to those contained in the pool
   // note: storage pool is used from the parent dir, if not set explicitely
   if (storagePoolId == StoragePoolStore::INVALID_POOL_ID)
      storagePoolId = stripePattern->getStoragePoolId();

   if(stripePattern->getPatternType() == StripePatternType_BuddyMirror)
   { // buddy mirror is special case, because no randmoninternode support and NodeCapacityPool

      // get the storage pool, which is set and restrict targets to those contained in the pool
      StoragePoolPtr storagePool = app->getStoragePoolStore()->getPool(storagePoolId);
      if (!storagePool)
      {
         // means there won't be allowed targets => file will not be created
         LOG(GENERAL, WARNING, "Stripe pattern has storage pool ID set, which doesn't exist.",
                      ("dirID", id), ("poolID", stripePattern->getStoragePoolId()));
         return NULL;
      }

      NodeCapacityPools* capacityPools = storagePool->getBuddyCapacityPools();

      /* (note: chooserType random{inter,intra}node is not supported by buddy mirrors, because we
         can't do the internal node-based grouping for buddy mirrors, so we fallback to random in
         in this case) */
      if( (chooserType == TargetChooserType_RANDOMIZED) ||
          (chooserType == TargetChooserType_RANDOMINTERNODE) ||
         //  (chooserType == TargetChooserType_RANDOMINTRANODE) ||
          (preferredTargets && !preferredTargets->empty() ) )
      { // randomized chooser, the only chooser that currently supports preferredTargets
         capacityPools->chooseStorageTargets(desiredNumTargets, minNumRequiredTargets,
                                             preferredTargets, &stripeTargets);
      }
      else
      { // round robin or randomized round robin chooser
         capacityPools->chooseStorageTargetsRoundRobin(desiredNumTargets, &stripeTargets);

         if(chooserType == TargetChooserType_RANDOMROBIN)
            random_shuffle(stripeTargets.begin(), stripeTargets.end() ); // randomize result vector
      }
   }
   else
   {
      // get the storage pool, which is set and restrict targets to those contained in the pool
      StoragePoolPtr storagePool = app->getStoragePoolStore()->getPool(storagePoolId);
      if (!storagePool)
      {
         // means there won't be allowed targets => file will not be created
         LOG(GENERAL, WARNING, "Stripe pattern has storage pool ID set, which doesn't exist.",
                      ("dirID", id), ("poolID", stripePattern->getStoragePoolId()));
         return NULL;
      }

      TargetCapacityPools* capacityPools = storagePool->getTargetCapacityPools();

      if(chooserType == TargetChooserType_RANDOMIZED ||
         (preferredTargets && !preferredTargets->empty() ) )
      { // randomized chooser, the only chooser that currently supports preferredTargets
         capacityPools->chooseStorageTargets(desiredNumTargets, minNumRequiredTargets,
                                             preferredTargets, &stripeTargets);
      }
      else
      if(chooserType == TargetChooserType_RANDOMINTERNODE)
      { // select targets from different nodeIDs
         capacityPools->chooseTargetsInterdomain(desiredNumTargets, minNumRequiredTargets,
                                                 &stripeTargets);
      }
      else
      if(chooserType == TargetChooserType_RANDOMINTRANODE)
      { // select targets from different nodeIDs
         capacityPools->chooseTargetsIntradomain(desiredNumTargets, minNumRequiredTargets,
                                                 &stripeTargets);
      }
      else
      { // round robin or randomized round robin chooser
         capacityPools->chooseStorageTargetsRoundRobin(desiredNumTargets, &stripeTargets);

         if(chooserType == TargetChooserType_RANDOMROBIN)
            random_shuffle(stripeTargets.begin(), stripeTargets.end() ); // randomize result vector
      }
   }

   // check whether we got enough targets...
   if(unlikely(stripeTargets.size() < minNumRequiredTargets) )
      return NULL;

   // clone the dir pattern with new stripeTargets...

   filePattern = stripePattern->clone();
   *filePattern->getStripeTargetIDsModifyable() = stripeTargets;

   if(cfg->getTuneRotateMirrorTargets() &&
      (stripePattern->getPatternType() == StripePatternType_Raid10) )
   {
      // rotate the selected target by half their number and use the rotated vector as mirror
      // targets. The advantage here is that we can use an odd number of stripe targets.
      // Example: stripeTargets=1,2,3,4,5; mirrorTargets=3,4,5,1,2
      std::vector<uint16_t> mirrorTargets;

      mirrorTargets.insert(mirrorTargets.end(),
            stripeTargets.begin() + (stripeTargets.size() / 2), stripeTargets.end());
      mirrorTargets.insert(mirrorTargets.end(),
            stripeTargets.begin(), stripeTargets.begin() + (stripeTargets.size() / 2));

      *((Raid10Pattern*) filePattern)->getMirrorTargetIDsModifyable() = mirrorTargets;
   }

   filePattern->setDefaultNumTargets(desiredNumTargets);
   filePattern->setChunkSize(chunksize);

   return filePattern;
}

StripePattern* DirInode::getStripePatternClone()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

   StripePattern* pattern = stripePattern->clone();

   safeLock.unlock();

   return pattern;
}

/**
 * @param newPattern will be cloned
 */
FhgfsOpsErr DirInode::setStripePattern(const StripePattern& newPattern, uint32_t actorUID)
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);

   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_INTERNAL;

   if (actorUID != 0 && statData.getUserID() != actorUID)
      return FhgfsOpsErr_PERM;

   StripePattern* oldPattern;

   if (actorUID != 0)
   {
      // Note there is no way to distinguish if the pool ID or pattern type in the pattern was
      // actually updated by the user or if CTL just set it to the previous value. Only if either
      // changes return a permissions error. This does mean non-root users that try to set these to
      // their current values will not see a permissions error.
      if (stripePattern->getStoragePoolId() != newPattern.getStoragePoolId()) {
         return FhgfsOpsErr_PERM;
      }
      if (stripePattern->getPatternType() != newPattern.getPatternType()) {
         return FhgfsOpsErr_PERM;
      }    
      oldPattern = stripePattern->clone();
      stripePattern->setDefaultNumTargets(newPattern.getDefaultNumTargets());
      stripePattern->setChunkSize(newPattern.getChunkSize());
   }
   else
   {
      oldPattern = this->stripePattern;
      this->stripePattern = newPattern.clone();
   }

   if(!storeUpdatedMetaDataUnlocked() )
   { // failed to update metadata => restore old value
      delete(this->stripePattern); // delete newPattern-clone
      this->stripePattern = oldPattern;
      return FhgfsOpsErr_INTERNAL;
   }
   else
   { // sucess
      delete(oldPattern);
   }

   lock.unlock();

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Note: See listIncrementalEx for comments; this is just a wrapper for it that doen't retrieve the
 * direntry types.
 */
FhgfsOpsErr DirInode::listIncremental(int64_t serverOffset,
   unsigned maxOutNames, StringList* outNames, int64_t* outNewServerOffset)
{
   ListIncExOutArgs outArgs(outNames, NULL, NULL, NULL, outNewServerOffset);

   return listIncrementalEx(serverOffset, maxOutNames, true, outArgs);
}

/**
 * Note: Returns only a limited number of entries.
 *
 * Note: serverOffset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset (similar to telldir/seekdir).
 *
 * Note: You have reached the end of the directory when success is returned and
 * "outNames.size() != maxOutNames".
 *
 * @param serverOffset zero-based offset
 * @param maxOutNames the maximum number of entries that will be added to the outNames
 * @param filterDots true to not return "." and ".." entries.
 * @param outArgs outNewOffset is only valid if return value indicates success.
 */
FhgfsOpsErr DirInode::listIncrementalEx(int64_t serverOffset,
   unsigned maxOutNames, bool filterDots, ListIncExOutArgs& outArgs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr listRes = entries.listIncrementalEx(serverOffset, maxOutNames, filterDots, outArgs);

   safeLock.unlock(); // U N L O C K

   return listRes;
}

/**
 * Note: Returns only a limited number of dentry-by-ID file names
 *
 * Note: serverOffset is an internal value and should not be assumed to be just 0, 1, 2, 3, ...;
 * so make sure you use either 0 (at the beginning) or something that has been returned by this
 * method as outNewOffset.
 *
 * Note: this function was written for fsck
 *
 * @param serverOffset zero-based offset
 * @param incrementalOffset zero-based offset; only used if serverOffset is -1
 * @param maxOutNames the maximum number of entries that will be added to the outNames
 * @param outArgs outNewOffset is only valid if return value indicates success, outEntryTypes is
 * not used (NULL), outNames is required.
 */
FhgfsOpsErr DirInode::listIDFilesIncremental(int64_t serverOffset, uint64_t incrementalOffset,
   unsigned maxOutNames, ListIncExOutArgs& outArgs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr listRes = entries.listIDFilesIncremental(serverOffset, incrementalOffset,
      maxOutNames, outArgs);

   safeLock.unlock(); // U N L O C K

   return listRes;
}

/**
 * Check whether an entry with the given name exists in this directory.
 */
bool DirInode::exists(const std::string& entryName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   bool retVal = entries.exists(entryName);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * @param outInodeMetaData might be NULL
 * @return DirEntryType_INVALID if entry does not exist
 */
FhgfsOpsErr DirInode::getEntryData(const std::string& entryName, EntryInfo* outInfo,
   FileInodeStoreData* outInodeMetaData)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   FhgfsOpsErr retVal = entries.getEntryData(entryName, outInfo, outInodeMetaData);

   if (retVal == FhgfsOpsErr_SUCCESS && DirEntryType_ISREGULARFILE(outInfo->getEntryType() ) )
   {
      bool inStore = fileStore.isInStore(outInfo->getEntryID() );
      if (inStore)
      {  // hint for the caller not to rely on outInodeMetaData
         retVal = FhgfsOpsErr_DYNAMICATTRIBSOUTDATED;
      }
   }

   safeLock.unlock(); // U N L O C K

   return retVal;
}


/**
 * Create new directory entry in the directory given by DirInode
 */
FhgfsOpsErr DirInode::makeDirEntry(DirEntry& entry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   // we always delete the entry from this method
   FhgfsOpsErr mkRes = makeDirEntryUnlocked(&entry);

   safeLock.unlock(); // U N L O C K

   return mkRes;
}

FhgfsOpsErr DirInode::makeDirEntryUnlocked(DirEntry* entry)
{
   FhgfsOpsErr mkRes = FhgfsOpsErr_INTERNAL;

   DirEntryType entryType = entry->getEntryType();
   if (unlikely( (!DirEntryType_ISFILE(entryType) && (!DirEntryType_ISDIR(entryType) ) ) ) )
      goto out;

   // load DirInode on demand if required, we need it now
   if (!loadIfNotLoadedUnlocked())
   {
      mkRes = FhgfsOpsErr_PATHNOTEXISTS;
      goto out;
   }

   mkRes = this->entries.makeEntry(entry);
   if(mkRes == FhgfsOpsErr_SUCCESS)
   { // entry successfully created

      if (DirEntryType_ISDIR(entryType) )
      {
         // update our own dirInode
         increaseNumSubDirsAndStoreOnDisk();
      }
      else
      {
         // update our own dirInode
         increaseNumFilesAndStoreOnDisk();
      }

   }

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

out:
   return mkRes;
}

FhgfsOpsErr DirInode::linkFilesInDirUnlocked(const std::string& fromName, FileInode& fromInode,
   const std::string& toName)
{
   FhgfsOpsErr linkRes = entries.linkEntryInDir(fromName, toName);

   if(linkRes == FhgfsOpsErr_SUCCESS)
   {
      // ignore the return code, time stamps are not that important
      increaseNumFilesAndStoreOnDisk();
   }

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return linkRes;
}


/**
 * Link an existing inode (stored in dir-entry format) to our directory. So add a new dirEntry.
 *
 * Used for example for linking an (unlinked/deleted) file-inode to the disposal dir.
 */
FhgfsOpsErr DirInode::linkFileInodeToDir(const std::string& inodePath, const std::string& fileName)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   // we always delete the entry from this method
   FhgfsOpsErr retVal = linkFileInodeToDirUnlocked(inodePath, fileName);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

FhgfsOpsErr DirInode::linkFileInodeToDirUnlocked(const std::string& inodePath,
   const std::string& fileName)
{
   bool loadRes = loadIfNotLoadedUnlocked();
   if (!loadRes)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr retVal = this->entries.linkInodeToDir(inodePath, fileName);

   if (retVal == FhgfsOpsErr_SUCCESS)
      increaseNumFilesAndStoreOnDisk();

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return retVal;
}

FhgfsOpsErr DirInode::removeDir(const std::string& entryName, DirEntry** outDirEntry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr rmRes = removeDirUnlocked(entryName, outDirEntry);

   safeLock.unlock(); // U N L O C K

   return rmRes;
}

/**
 * @param outDirEntry is object of the removed dirEntry, maybe NULL of the caller does not need it
 */
FhgfsOpsErr DirInode::removeDirUnlocked(const std::string& entryName, DirEntry** outDirEntry)
{
   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr rmRes = entries.removeDir(entryName, outDirEntry);

   if(rmRes == FhgfsOpsErr_SUCCESS)
   {
      if (unlikely (!decreaseNumSubDirsAndStoreOnDisk() ) )
         rmRes = FhgfsOpsErr_INTERNAL;
   }

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return rmRes;
}

/**
 *
 * @param unlinkEntryName If true do not try to unlink the dentry-name entry, entryName even
 *    might not be set.
 * @param outFile will be set to the unlinked file and the object must then be deleted by the caller
 * (can be NULL if the caller is not interested in the file)
 */
FhgfsOpsErr DirInode::renameDirEntry(const std::string& fromName, const std::string& toName,
   DirEntry* overWriteEntry)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr renameRes = renameDirEntryUnlocked(fromName, toName, overWriteEntry);

   safeLock.unlock(); // U N L O C K

   return renameRes;
}


/**
 * Rename an entry in the same directory.
 */
FhgfsOpsErr DirInode::renameDirEntryUnlocked(const std::string& fromName, const std::string& toName,
   DirEntry* overWriteEntry)
{
   const char* logContext = "DirInode rename entry";

   FhgfsOpsErr renameRes = entries.renameEntry(fromName, toName);

   if(renameRes == FhgfsOpsErr_SUCCESS)
   {
      if (overWriteEntry)
      {
         if (DirEntryType_ISDIR(overWriteEntry->getEntryType() )  )
            this->numSubdirs--;
         else
            this->numFiles--;
      }

      // ignore the return code, time stamps are not that important
      updateTimeStampsAndStoreToDisk(logContext);
   }

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return renameRes;
}

/**
 * @param inEntry should be provided if available for performance, but may be NULL if
 *    unlinkTypeFlags == DirEntry_UNLINK_FILENAME_ONLY or unlinkTypeFlags == DirEntry_UNLINK_ID_AND_FILENAME
 * @param unlinkEntryName If false do not try to unlink the dentry-name entry, entryName even
 *    might not be set (but inEntry must be set in this case).
 * @param outFile will be set to the unlinked file and the object must then be deleted by the caller
 * (can be NULL if the caller is not interested in the file)
 */
FhgfsOpsErr DirInode::unlinkDirEntry(const std::string& entryName, DirEntry* entry,
   unsigned unlinkTypeFlags)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr unlinkRes = unlinkDirEntryUnlocked(entryName, entry, unlinkTypeFlags);

   safeLock.unlock(); // U N L O C K

   return unlinkRes;
}

/**
 * @inEntry should be provided if available for performance reasons, but may be NULL (in which case
 * unlinkTypeFlags must be != DirEntry_UNLINK_ID_ONLY)
 */
FhgfsOpsErr DirInode::unlinkDirEntryUnlocked(const std::string& entryName, DirEntry* inEntry,
   unsigned unlinkTypeFlags)
{
   const char* logContext = "DirInode unlink entry";

   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   DirEntry* entry;
   DirEntry loadedEntry(entryName);

   if (!inEntry)
   {
      if (unlikely(!(unlinkTypeFlags & DirEntry_UNLINK_FILENAME) ) )
      {
         LogContext(logContext).logErr("Bug: inEntry missing!");
         LogContext(logContext).logBacktrace();
         return FhgfsOpsErr_INTERNAL;
      }

      bool getRes = getDentryUnlocked(entryName, loadedEntry);
      if (!getRes)
         return FhgfsOpsErr_PATHNOTEXISTS;

      entry = &loadedEntry;
   }
   else
      entry = inEntry;

   FhgfsOpsErr unlinkRes = this->entries.unlinkDirEntry(entryName, entry, unlinkTypeFlags);

   if (unlinkRes != FhgfsOpsErr_SUCCESS)
      goto out;

   if (unlinkTypeFlags & DirEntry_UNLINK_FILENAME)
   {
      bool decRes;

      if (DirEntryType_ISDIR(entry->getEntryType() ) )
         decRes = decreaseNumSubDirsAndStoreOnDisk();
      else
         decRes = decreaseNumFilesAndStoreOnDisk();

      if (unlikely(!decRes) )
      {
         LogContext(logContext).logErr("Unlink succeeded, but failed to store updated DirInode"
            " DirID: " + this->getID() );

         // except of this warning we ignore this error, as the unlink otherwise succeeded
      }
   }

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

out:
   return unlinkRes;
}

FhgfsOpsErr DirInode::refreshMetaInfo()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr retVal = refreshMetaInfoUnlocked();

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Re-computes number of subentries.
 *
 * Note: Intended to be called only when you have reason to believe that the stored metadata is
 * not correct (eg after an upgrade that introduced new metadata information).
 */
FhgfsOpsErr DirInode::refreshMetaInfoUnlocked()
{

   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr retVal = refreshSubentryCountUnlocked();
   if(retVal == FhgfsOpsErr_SUCCESS)
   {
      if(!storeUpdatedMetaDataUnlocked() )
         retVal = FhgfsOpsErr_INTERNAL;
   }

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return retVal;
}

/**
 * Reads all stored entry links (via entries::listInc() ) to re-compute number of subentries.
 *
 * Note: Intended to be called only when you have reason to believe that the stored metadata is
 * not correct (eg after an upgrade that introduced new metadata information).
 * Note: Does not update persistent metadata.
 * Note: Unlocked, so hold the write lock when calling this.
 */
FhgfsOpsErr DirInode::refreshSubentryCountUnlocked()
{
   const char* logContext = "Directory (refresh entry count)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   unsigned currentNumSubdirs = 0;
   unsigned currentNumFiles = 0;

   unsigned maxOutNames = 128;
   StringList names;
   UInt8List entryTypes;
   int64_t serverOffset = 0;
   size_t numOutEntries; // to avoid inefficient calls to list::size.

   // query contents
   ListIncExOutArgs outArgs(&names, &entryTypes, NULL, NULL, &serverOffset);

   do
   {
      names.clear();
      entryTypes.clear();

      FhgfsOpsErr listRes = entries.listIncrementalEx(serverOffset, maxOutNames, true, outArgs);
      if(listRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).logErr(
            std::string("Unable to fetch dir contents for dirID: ") + id);
         retVal = listRes;
         break;
      }

      // walk over returned entries

      StringListConstIter namesIter = names.begin();
      UInt8ListConstIter typesIter = entryTypes.begin();
      numOutEntries = 0;

      for( /* using namesIter, typesIter, numOutEntries */;
          (namesIter != names.end() ) && (typesIter != entryTypes.end() );
          namesIter++, typesIter++, numOutEntries++)
      {
         if(DirEntryType_ISDIR(*typesIter) )
            currentNumSubdirs++;
         else
         if(DirEntryType_ISFILE(*typesIter) )
            currentNumFiles++;
         else
         { // invalid entry => log error, but continue
            LogContext(logContext).logErr(std::string("Unable to read entry type for name: '") +
               *namesIter + "' "  "(dirID: " + id + ")");
         }
      }

   } while(numOutEntries == maxOutNames);

   if(retVal == FhgfsOpsErr_SUCCESS)
   { // update in-memory counters (but we leave persistent updates to the caller)
      this->numSubdirs = currentNumSubdirs;
      this->numFiles = currentNumFiles;
   }

   return retVal;
}


/*
 * Note: Current object state is used for the serialization.
 */
FhgfsOpsErr DirInode::storeInitialMetaData()
{
   FhgfsOpsErr dirRes = DirEntryStore::mkDentryStoreDir(this->id, this->getIsBuddyMirrored());
   if(dirRes != FhgfsOpsErr_SUCCESS)
      return dirRes;

   FhgfsOpsErr fileRes = storeInitialMetaDataInode();
   if(unlikely(fileRes != FhgfsOpsErr_SUCCESS) )
   {
      if (unlikely(fileRes == FhgfsOpsErr_EXISTS) )
      {
         // there must have been some kind of race as dirRes was successful
         fileRes = FhgfsOpsErr_SUCCESS;
      }
      else
         DirEntryStore::rmDirEntryStoreDir(this->id, this->getIsBuddyMirrored()); // remove dir
   }

   return fileRes;
}

FhgfsOpsErr DirInode::storeInitialMetaData(const CharVector& defaultACLXAttr,
         const CharVector& accessACLXAttr)
{
   FhgfsOpsErr res = storeInitialMetaData();

   if (res != FhgfsOpsErr_SUCCESS)
      return res;

   // xattr updates done here are also resynced if storeInitialMetaData enqueued the inode
   if (!defaultACLXAttr.empty())
      res = setXAttr(nullptr, PosixACL::defaultACLXAttrName, defaultACLXAttr, 0);

   if (res != FhgfsOpsErr_SUCCESS)
      return res;

   if (!accessACLXAttr.empty())
      res = setXAttr(nullptr, PosixACL::accessACLXAttrName, accessACLXAttr, 0);

   return res;
}

/*
 * Creates the initial metadata inode for this directory.
 *
 * Note: Current object state is used for the serialization.
 */
FhgfsOpsErr DirInode::storeInitialMetaDataInode()
{
   const char* logContext = "Directory (store initial metadata file)";

   App* app = Program::getApp();
   const Path* inodePath =
      getIsBuddyMirrored() ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   bool useXAttrs = app->getConfig()->getStoreUseExtendedAttribs();

   char buf[META_SERBUF_SIZE];
   Serializer ser(buf, sizeof(buf));

   // create file

   int openFlags = O_CREAT|O_EXCL|O_WRONLY;
   mode_t openMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

   int fd = open(metaFilename.c_str(), openFlags, openMode);
   if(fd == -1)
   { // error
      if(errno == EEXIST)
         retVal = FhgfsOpsErr_EXISTS;
      else
      {
         LogContext(logContext).logErr("Unable to create dir metadata inode " + metaFilename +
            ". " + "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;
      }

      goto error_donothing;
   }

   // alloc buf and serialize

   DiskMetaData::serializeDirInode(ser, *this);
   if (!ser.good())
   {
      LOG(GENERAL, ERR, "Serialized metadata is larger than serialization buffer size.",
            id, parentDirID, metaFilename,
            ("Data size", ser.size()),
            ("Buffer size", META_SERBUF_SIZE));

      retVal = FhgfsOpsErr_INTERNAL;
      goto error_closefile;
   }

   // write data to file

   if(useXAttrs)
   { // extended attribute
      int setRes = fsetxattr(fd, META_XATTR_NAME, buf, ser.size(), 0);

      if(unlikely(setRes == -1) )
      { // error
         if(errno == ENOTSUP)
            LogContext(logContext).logErr("Unable to store directory xattr metadata: " +
               metaFilename + ". " +
               "Did you enable extended attributes (user_xattr) on the underlying file system?");
         else
            LogContext(logContext).logErr("Unable to store directory xattr metadata: " +
               metaFilename + ". " + "SysErr: " + System::getErrString() );

         retVal = FhgfsOpsErr_INTERNAL;

         goto error_closefile;
      }
   }
   else
   { // normal file content
      ssize_t writeRes = write(fd, buf, ser.size());

      if(writeRes != (ssize_t)ser.size())
      {
         LogContext(logContext).logErr("Unable to store directory metadata: " + metaFilename +
            ". " + "SysErr: " + System::getErrString() );
         retVal = FhgfsOpsErr_INTERNAL;

         goto error_closefile;
      }
   }

   close(fd);

   LOG_DEBUG(logContext, Log_DEBUG, "Directory metadata inode stored: " + metaFilename);

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(metaFilename, MetaSyncFileType::Inode);

   return retVal;


   // error compensation
error_closefile:
   close(fd);
   unlink(metaFilename.c_str() );

error_donothing:

   return retVal;
}

/**
 * Note: Wrapper/chooser for storeUpdatedMetaDataBufAsXAttr/Contents.
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirInode::storeUpdatedMetaDataBuf(char* buf, unsigned bufLen)
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   if(useXAttrs)
      return storeUpdatedMetaDataBufAsXAttr(buf, bufLen);

   return storeUpdatedMetaDataBufAsContents(buf, bufLen);
}

/**
 * Note: Don't call this directly, use the wrapper storeUpdatedMetaDataBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirInode::storeUpdatedMetaDataBufAsXAttr(char* buf, unsigned bufLen)
{
   const char* logContext = "Directory (store updated xattr metadata)";

   App* app = Program::getApp();
   const Path* inodePath =
      getIsBuddyMirrored() ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);

   // write data to file

   int setRes = setxattr(metaFilename.c_str(), META_XATTR_NAME, buf, bufLen, 0);

   if(unlikely(setRes == -1) )
   { // error
      LogContext(logContext).logErr("Unable to write directory metadata update: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   LOG_DEBUG(logContext, 4, "Directory metadata update stored: " + id);

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(metaFilename, MetaSyncFileType::Inode);

   return true;
}

/**
 * Stores the update to a sparate file first and then renames it.
 *
 * Note: Don't call this directly, use the wrapper storeUpdatedMetaDataBuf().
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirInode::storeUpdatedMetaDataBufAsContents(char* buf, unsigned bufLen)
{
   const char* logContext = "Directory (store updated metadata)";

   App* app = Program::getApp();
   const Path* inodePath =
      getIsBuddyMirrored() ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
   std::string metaUpdateFilename(metaFilename + META_UPDATE_EXT_STR);

   ssize_t writeRes;
   int renameRes;

   // open file (create it, but not O_EXCL because a former update could have failed)
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(metaUpdateFilename.c_str(), openFlags, 0644);
   if(fd == -1)
   { // error
      if(errno == ENOSPC)
      { // no free space => try again with update in-place
         LogContext(logContext).log(Log_DEBUG, "No space left to create update file. Trying update "
            "in-place: " + metaUpdateFilename + ". " + "SysErr: " + System::getErrString() );

         return storeUpdatedMetaDataBufAsContentsInPlace(buf, bufLen);
      }

      LogContext(logContext).logErr("Unable to create directory metadata update file: " +
         metaUpdateFilename + ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   // metafile created => store meta data
   writeRes = write(fd, buf, bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      if( (writeRes >= 0) || (errno == ENOSPC) )
      { // no free space => try again with update in-place
         LogContext(logContext).log(Log_DEBUG, "No space left to write update file. Trying update "
            "in-place: " + metaUpdateFilename + ". " + "SysErr: " + System::getErrString() );

         close(fd);
         unlink(metaUpdateFilename.c_str() );

         return storeUpdatedMetaDataBufAsContentsInPlace(buf, bufLen);
      }

      LogContext(logContext).logErr("Unable to store directory metadata update: " + metaFilename +
         ". " + "SysErr: " + System::getErrString() );

      goto error_closefile;
   }

   close(fd);

   renameRes = rename(metaUpdateFilename.c_str(), metaFilename.c_str() );
   if(renameRes == -1)
   {
      LogContext(logContext).logErr("Unable to replace old directory metadata file: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );

      goto error_unlink;
   }

   LOG_DEBUG(logContext, 4, "Directory metadata update stored: " + id);

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(metaFilename, MetaSyncFileType::Inode);

   return true;


   // error compensation
error_closefile:
   close(fd);

error_unlink:
   unlink(metaUpdateFilename.c_str() );

   return false;
}

/**
 * Stores the update directly to the current metadata file (instead of creating a separate file
 * first and renaming it).
 *
 * Note: Don't call this directly, it is automatically called by storeUpdatedMetaDataBufAsContents()
 * when necessary.
 *
 * @param buf the serialized object state that is to be stored
 */
bool DirInode::storeUpdatedMetaDataBufAsContentsInPlace(char* buf, unsigned bufLen)
{
   const char* logContext = "Directory (store updated metadata in-place)";

   App* app = Program::getApp();
   const Path* inodePath =
      getIsBuddyMirrored() ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);

   int fallocRes;
   ssize_t writeRes;
   int truncRes;

   // open file (create it, but not O_EXCL because a former update could have failed)
   int openFlags = O_CREAT|O_WRONLY;

   int fd = open(metaFilename.c_str(), openFlags, 0644);
   if(fd == -1)
   { // error
      LogContext(logContext).logErr("Unable to open directory metadata file: " +
         metaFilename + ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   // make sure we have enough room to write our update
   fallocRes = posix_fallocate(fd, 0, bufLen); // (note: posix_fallocate does not set errno)
   if(fallocRes == EBADF)
   { // special case for XFS bug
      struct stat statBuf;
      int statRes = fstat(fd, &statBuf);

      if (statRes == -1)
      {
         LogContext(logContext).log(Log_WARNING, "Unexpected error: fstat() failed with SysErr: "
            + System::getErrString(errno));
         goto error_closefile;
      }

      if (statBuf.st_size < bufLen)
      {
         LogContext(logContext).log(Log_WARNING, "File space allocation ("
            + StringTk::intToStr(bufLen)  + ") for metadata update failed: " + metaFilename + ". " +
            "SysErr: " + System::getErrString(fallocRes) + " "
            "statRes: " + StringTk::intToStr(statRes) + " "
            "oldSize: " + StringTk::intToStr(statBuf.st_size));
         goto error_closefile;
      }
      else
      { // // XFS bug! We only return an error if statBuf.st_size < bufLen. Ingore fallocRes then
         LOG_DEBUG(logContext, Log_SPAM, "Ignoring kernel file system bug: "
            "posix_fallocate() failed for len < filesize");
      }
   }
   else
   if (fallocRes != 0)
   { // default error handling if posix_fallocate() failed
      LogContext(logContext).log(Log_WARNING, "File space allocation ("
         + StringTk::intToStr(bufLen)  + ") for metadata update failed: " +  metaFilename + ". " +
         "SysErr: " + System::getErrString(fallocRes));
      goto error_closefile;
   }

   // metafile created => store meta data
   writeRes = write(fd, buf, bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      LogContext(logContext).logErr("Unable to store directory metadata update: " + metaFilename +
         ". " + "SysErr: " + System::getErrString() );

      goto error_closefile;
   }

   // truncate in case the update lead to a smaller file size
   truncRes = ftruncate(fd, bufLen);
   if(truncRes == -1)
   { // ignore trunc errors
      LogContext(logContext).log(Log_WARNING, "Unable to truncate metadata file (strange, but "
         "proceeding anyways): " + metaFilename + ". " + "SysErr: " + System::getErrString() );
   }


   close(fd);

   LOG_DEBUG(logContext, 4, "Directory metadata in-place update stored: " + id);

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(metaFilename, MetaSyncFileType::Inode);

   return true;


   // error compensation
error_closefile:
   close(fd);

   return false;
}


bool DirInode::storeUpdatedMetaDataUnlocked()
{
   std::string logContext = "Write Meta Inode";

   if (unlikely(!this->isLoaded) )
   {
      LogContext(logContext).logErr("Bug: Inode data not loaded yet.");
      LogContext(logContext).logBacktrace();

      return false;
   }

   #ifdef DEBUG_MUTEX_LOCKING
      if (unlikely(!this->rwlock.isRWLocked() ) )
      {
         LogContext(logContext).logErr("Bug: Inode not rw-locked.");
         LogContext(logContext).logBacktrace();

         return false;
      }
   #endif

   char buf[META_SERBUF_SIZE];
   Serializer ser(buf, sizeof(buf));

   DiskMetaData::serializeDirInode(ser, *this);
   if (!ser.good())
   {
      LOG(GENERAL, ERR, "Serialized metadata is larger than serialization buffer size.",
            id, parentDirID,
            ("Data size", ser.size()),
            ("Buffer size", META_SERBUF_SIZE));
      LogContext(logContext).logBacktrace();
      return false;
   }

   return storeUpdatedMetaDataBuf(buf, ser.size());
}

/**
 * Note: Assumes that the caller already verified that the directory is empty
 */
bool DirInode::removeStoredMetaDataFile(const std::string& id, bool isBuddyMirrored)
{
   const char* logContext = "Directory (remove stored metadata)";

   App* app = Program::getApp();
   const Path* inodePath =
      isBuddyMirrored ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string metaFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);

   // delete metadata file

   int unlinkRes = unlink(metaFilename.c_str() );
   if(unlinkRes == -1)
   { // error
      LogContext(logContext).logErr("Unable to delete directory metadata file: " + metaFilename +
         ". " + "SysErr: " + System::getErrString() );

      if (errno != ENOENT)
         return false;
   }

   LOG_DEBUG(logContext, 4, "Directory metadata deleted: " + metaFilename);

   if (isBuddyMirrored)
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addDeletion(metaFilename, MetaSyncFileType::Inode);

   return true;
}

bool DirInode::loadIfNotLoaded()
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

   bool loadRes = loadIfNotLoadedUnlocked();

   safeLock.unlock();

   return loadRes;
}

void DirInode::invalidate()
{
   UniqueRWLock lock(rwlock, SafeRWLock_WRITE);
   isLoaded = false;
}


/**
 * Load the DirInode from disk if it was notalready loaded before.
 *
 * @return true if loading not required or loading successfully, false if loading from disk failed.
 */
bool DirInode::loadIfNotLoadedUnlocked()
{
   if (!this->isLoaded)
   { // So the object was already created before without loading the inode from disk, do that now.
      bool loadSuccess = loadFromFile();
      if (!loadSuccess)
      {
         const char* logContext = "Loading DirInode on demand";
         std::string msg = "Loading DirInode failed dir-ID: ";
         LOG_DEBUG_CONTEXT(LogContext(logContext), Log_DEBUG, msg + this->id);
         IGNORE_UNUSED_VARIABLE(logContext);

         return false;
      }
   }

   return true;
}

/**
 * Note: Wrapper/chooser for loadFromFileXAttr/Contents.
 */
bool DirInode::loadFromFile()
{
   bool useXAttrs = Program::getApp()->getConfig()->getStoreUseExtendedAttribs();

   bool loadRes;
   if(useXAttrs)
      loadRes = loadFromFileXAttr();
   else
      loadRes = loadFromFileContents();

   if (loadRes)
      this->isLoaded = true;

   return loadRes;
}


/**
 * Note: Don't call this directly, use the wrapper loadFromFile().
 */
bool DirInode::loadFromFileXAttr()
{
   const char* logContext = "Directory (load from xattr file)";

   App* app = Program::getApp();

   const Path* inodePath =
      getIsBuddyMirrored() ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);

   bool retVal = false;

   char buf[META_SERBUF_SIZE];

   ssize_t getRes = getxattr(inodeFilename.c_str(), META_XATTR_NAME, buf, META_SERBUF_SIZE);
   if(getRes > 0)
   { // we got something => deserialize it
      Deserializer des(buf, getRes);
      DiskMetaData::deserializeDirInode(des, *this);
      if(unlikely(!des.good()))
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize metadata in file: " + inodeFilename);
         goto error_exit;
      }

      retVal = true;
   }
   else
   if( (getRes == -1) && (errno == ENOENT) )
   { // file not exists
      LOG_DEBUG(logContext, Log_DEBUG, "Metadata file not exists: " +
         inodeFilename + ". " + "SysErr: " + System::getErrString() );
   }
   else
   { // unhandled error
      LogContext(logContext).logErr("Unable to open/read xattr metadata file: " +
         inodeFilename + ". " + "SysErr: " + System::getErrString() );
   }


error_exit:

   return retVal;
}

/**
 * Note: Don't call this directly, use the wrapper loadFromFile().
 */
bool DirInode::loadFromFileContents()
{
   const char* logContext = "Directory (load from file)";

   App* app = Program::getApp();
   const Path* inodePath =
      getIsBuddyMirrored() ? app->getBuddyMirrorInodesPath() : app->getInodesPath();
   std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);

   bool retVal = false;

   int openFlags = O_NOATIME | O_RDONLY;

   int fd = open(inodeFilename.c_str(), openFlags, 0);
   if(fd == -1)
   { // open failed
      if(errno != ENOENT)
         LogContext(logContext).logErr("Unable to open metadata file: " + inodeFilename +
            ". " + "SysErr: " + System::getErrString() );

      return false;
   }

   char buf[META_SERBUF_SIZE];
   int readRes = read(fd, buf, META_SERBUF_SIZE);
   if(readRes <= 0)
   { // reading failed
      LogContext(logContext).logErr("Unable to read metadata file: " + inodeFilename + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   {
      Deserializer des(buf, readRes);
      DiskMetaData::deserializeDirInode(des, *this);
      if (!des.good())
      { // deserialization failed
         LogContext(logContext).logErr("Unable to deserialize metadata in file: " + inodeFilename);
      }
      else
      { // success
         retVal = true;
      }
   }

   close(fd);

   return retVal;
}


DirInode* DirInode::createFromFile(const std::string& id, bool isBuddyMirrored)
{
   DirInode* newDir = new DirInode(id, isBuddyMirrored);

   bool loadRes = newDir->loadFromFile();
   if(!loadRes)
   {
      delete(newDir);
      return NULL;
   }

   newDir->entries.setParentID(id, isBuddyMirrored);

   return newDir;
}

/**
 * Get directory stat information
 *
 * @param outParentNodeID may NULL if the caller is not interested
 */
FhgfsOpsErr DirInode::getStatData(StatData& outStatData,
   NumNodeID* outParentNodeID, std::string* outParentEntryID)
{
   // note: keep in mind that correct nlink count (2+numSubdirs) is absolutely required to not break
      // the "find" command, which uses optimizations based on nlink (see "find -noleaf" for
      // details). The other choice would be to set nlink=1, which find would reduce to -1
      // as is substracts 2 for "." and "..". -1 = max(unsigned) and makes find to check all
      // possible subdirs. But as we have the correct nlink anyway, we can provide it to find
      // and allow it to use non-posix optimizations.

   const char* logContext = "DirInode::getStatData";

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ);

   this->statData.setNumHardLinks(2 + numSubdirs); // +2 by definition (for "." and "<name>")

   this->statData.setFileSize(numSubdirs + numFiles); // just because we got those values anyway

   outStatData = this->statData;

   if (outParentNodeID)
   {
      *outParentNodeID = this->parentNodeID;

      #ifdef BEEGFS_DEBUG
         if (!outParentEntryID)
         {
            LogContext(logContext).logErr("Bug: outParentEntryID is unexpectedly NULL");
            LogContext(logContext).logBacktrace();
         }
      #endif
      IGNORE_UNUSED_VARIABLE(logContext);

      *outParentEntryID = this->parentDirID;
   }

   safeLock.unlock();

   return FhgfsOpsErr_SUCCESS;
}

/*
 * only used for testing at the moment
 */
void DirInode::setStatData(StatData& statData)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   this->statData = statData;

   this->numSubdirs = statData.getNumHardlinks() - 2;
   this->numFiles = statData.getFileSize() - this->numSubdirs;

   storeUpdatedMetaDataUnlocked();

   safeLock.unlock(); // U N L O C K

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }
}

/**
 * @param validAttribs SETATTR_CHANGE_...-Flags, might be 0 if only attribChangeTimeSecs
 *    shall be updated
 * @attribs  new attributes, but might be NULL if validAttribs == 0
 */
bool DirInode::setAttrData(int validAttribs, SettableFileAttribs* attribs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool retVal = setAttrDataUnlocked(validAttribs, attribs);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * See setAttrData() for documentation.
 */
bool DirInode::setAttrDataUnlocked(int validAttribs, SettableFileAttribs* attribs)
{
   bool success = true;


   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return false;

   // save old attribs
   SettableFileAttribs oldAttribs;
   oldAttribs = *(this->statData.getSettableFileAttribs() );

    this->statData.setAttribChangeTimeSecs(TimeAbs().getTimeval()->tv_sec);

   if(validAttribs)
   { // apply new attribs wrt flags...

      if(validAttribs & SETATTR_CHANGE_MODE)
         this->statData.setMode(attribs->mode);

      if(validAttribs & SETATTR_CHANGE_MODIFICATIONTIME)
      {
         /* only static value update required for storeUpdatedMetaDataUnlocked() */
         this->statData.setModificationTimeSecs(attribs->modificationTimeSecs);
      }

      if(validAttribs & SETATTR_CHANGE_LASTACCESSTIME)
      {
         /* only static value update required for storeUpdatedMetaDataUnlocked() */
         this->statData.setLastAccessTimeSecs(attribs->lastAccessTimeSecs);
      }

      if(validAttribs & SETATTR_CHANGE_USERID)
         this->statData.setUserID(attribs->userID);

      if(validAttribs & SETATTR_CHANGE_GROUPID)
         this->statData.setGroupID(attribs->groupID);
   }

   bool storeRes = storeUpdatedMetaDataUnlocked();
   if(!storeRes)
   { // failed to update metadata on disk => restore old values
      this->statData.setSettableFileAttribs(oldAttribs);

      success = false;
   }

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return success;
}

/**
 * Set/Update parent information (from the given entryInfo)
 */
FhgfsOpsErr DirInode::setDirParentAndChangeTime(EntryInfo* entryInfo, NumNodeID parentNodeID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   FhgfsOpsErr retVal = setDirParentAndChangeTimeUnlocked(entryInfo, parentNodeID);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * See setParentEntryID() for documentation.
 */
FhgfsOpsErr DirInode::setDirParentAndChangeTimeUnlocked(EntryInfo* entryInfo,
   NumNodeID parentNodeID)
{
   const char* logContext = "DirInode::setDirParentAndChangeTimeUnlocked";

   // load DirInode on demand if required, we need it now
   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
      return FhgfsOpsErr_PATHNOTEXISTS;

#ifdef BEEGFS_DEBUG
   LogContext(logContext).log(Log_DEBUG, "DirInode" + this->getID()   + " "
      "newParentDirID: "  + entryInfo->getParentEntryID()             + " "
      "newParentNodeID: " + parentNodeID.str()                        + ".");
#endif
   IGNORE_UNUSED_VARIABLE(logContext);

   this->parentDirID = entryInfo->getParentEntryID();
   this->parentNodeID = parentNodeID;

   // updates change time stamps and saves to disk
   bool attrRes = setAttrDataUnlocked(0, NULL);

   if (unlikely(!attrRes) )
      return FhgfsOpsErr_INTERNAL;

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return FhgfsOpsErr_SUCCESS;
}

std::pair<FhgfsOpsErr, StringVector> DirInode::listXAttr(const EntryInfo* file)
{
   return file
      ? XAttrTk::listUserXAttrs(MetaStorageTk::getMetaDirEntryIDPath(entries.getDirEntryPath())
            + "/" + file->getEntryID())
      : XAttrTk::listUserXAttrs(entries.getDirEntryPath());
}

/**
 * Get an extended attribute by name.
 * @returns first: FhgfsOpsErr_RANGE if size is smaller than xattr, FhgfsOpsErr_NODATA if there is
 *          no xattr by that name, FhgfsOpsErr_INTERNAL on any other errors, FhfgfsOpsErr_SUCCESS
 *          on success. second: xattr data, if first is FhgfsOpsErr_SUCCESS
 */
std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> DirInode::getXAttr(const EntryInfo* file,
   const std::string& xAttrName, size_t maxSize)
{
   return file
      ? XAttrTk::getUserXAttr(MetaStorageTk::getMetaDirEntryIDPath(entries.getDirEntryPath())
            + "/" + file->getEntryID(), xAttrName, maxSize)
      : XAttrTk::getUserXAttr(entries.getDirEntryPath(), xAttrName, maxSize);
}

FhgfsOpsErr DirInode::removeXAttr(EntryInfo* file, const std::string& xAttrName)
{
   const std::string& fileName = file
      ? MetaStorageTk::getMetaDirEntryIDPath(entries.getDirEntryPath())
            + "/" + file->getEntryID()
      : entries.getDirEntryPath();

   FhgfsOpsErr result = XAttrTk::removeUserXAttr(fileName, xAttrName);

   if (result == FhgfsOpsErr_SUCCESS)
   {
      if (file)
      {
         auto fileHandle = Program::getApp()->getMetaStore()->referenceFile(file);
         if (fileHandle)
         {
            fileHandle->updateInodeChangeTime(file);
            Program::getApp()->getMetaStore()->releaseFile(getID(), fileHandle);
         }
      }
      else
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);
         updateTimeStampsAndStoreToDisk(__func__);
      }
   }

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(fileName,
               file
                  ? MetaSyncFileType::Inode
                  : MetaSyncFileType::Directory);

   return result;
}

FhgfsOpsErr DirInode::setXAttr(EntryInfo* file, const std::string& xAttrName,
      const CharVector& xAttrValue, int flags, bool updateTimestamps)
{
   const std::string& fileName = file
      ? MetaStorageTk::getMetaDirEntryIDPath(entries.getDirEntryPath())
            + "/" + file->getEntryID()
      : entries.getDirEntryPath();

   FhgfsOpsErr result = XAttrTk::setUserXAttr(fileName, xAttrName, &xAttrValue[0],
         xAttrValue.size(), flags);

   if (result == FhgfsOpsErr_SUCCESS && updateTimestamps)
   {
      if (file)
      {
         auto fileHandle = Program::getApp()->getMetaStore()->referenceFile(file);
         if (fileHandle)
         {
            fileHandle->updateInodeChangeTime(file);
            Program::getApp()->getMetaStore()->releaseFile(getID(), fileHandle);
         }
      }
      else
      {
         UniqueRWLock lock(rwlock, SafeRWLock_WRITE);
         updateTimeStampsAndStoreToDisk(__func__);
      }
   }

   if (getIsBuddyMirrored())
      if (auto* resync = BuddyResyncer::getSyncChangeset())
         resync->addModification(fileName,
               file
                  ? MetaSyncFileType::Inode
                  : MetaSyncFileType::Directory);

   return result;
}


FhgfsOpsErr DirInode::setOwnerNodeID(const std::string& entryName, NumNodeID ownerNode)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool loadSuccess = loadIfNotLoadedUnlocked();
   if (!loadSuccess)
   {
      safeLock.unlock();
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   FhgfsOpsErr retVal = entries.setOwnerNodeID(entryName, ownerNode);

   safeLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * Move a dentry that has in inlined inode to the inode-hash directories
 * (probably an unlink() of an opened file).
 *
 * @param unlinkFileName Also unlink the fileName or only the ID file.
 */
bool DirInode::unlinkBusyFileUnlocked(const std::string& fileName, DirEntry* dentry,
   unsigned unlinkTypeFlags)
{
   const char* logContext = "unlink busy file";

   FhgfsOpsErr unlinkRes = this->entries.removeBusyFile(fileName, dentry, unlinkTypeFlags);
   if (unlinkRes == FhgfsOpsErr_SUCCESS)
   {
      if( (unlinkTypeFlags & DirEntry_UNLINK_FILENAME) &&
         unlikely(!decreaseNumFilesAndStoreOnDisk() ) )
         unlinkRes = FhgfsOpsErr_INTERNAL;
   }
   else
   {
      std::string msg = "Failed to remove file dentry. "
         "DirInode: " + this->id + " "
         "entryName: " + fileName + " "
         "entryID: " + dentry->getID() + " "
         "Error: " + boost::lexical_cast<std::string>(unlinkRes);

      LogContext(logContext).logErr(msg);
   }

   if (getIsBuddyMirrored())
   {
      if (auto* resync = BuddyResyncer::getSyncChangeset())
      {
         const Path* inodePath = Program::getApp()->getBuddyMirrorInodesPath();
         std::string inodeFilename = MetaStorageTk::getMetaInodePath(inodePath->str(), id);
         resync->addModification(inodeFilename, MetaSyncFileType::Inode);
      }
   }

   return unlinkRes;
}

/**
 * Set the buddyMirrored flag on the inode and all dentries/inlined file inode contained within.
 * If this method fails, the contained dentries and inodes will NOT be reset to their former value,
 * instead, a mix of mirrored and unmirrored items will be left behind.
 */
FhgfsOpsErr DirInode::setIsBuddyMirrored(const bool isBuddyMirrored)
{
   if (isBuddyMirrored)
      addFeatureFlag(DIRINODE_FEATURE_BUDDYMIRRORED);
   else
      removeFeatureFlag(DIRINODE_FEATURE_BUDDYMIRRORED);

   entries.setParentID(entries.getParentEntryID(), isBuddyMirrored);

   {
      int64_t offset = 0;

      while (true)
      {
         StringList dentries;
         ListIncExOutArgs args(&dentries, nullptr, nullptr, nullptr, &offset);

         const auto listRes = entries.listIncrementalEx(offset, 1, true, args);
         if (listRes != FhgfsOpsErr_SUCCESS)
            return listRes;

         if (dentries.empty())
            break;

         DirEntry dentry(dentries.front());

         if (!entries.getDentry(dentries.front(), dentry))
            return FhgfsOpsErr_PATHNOTEXISTS;

         if (!dentry.getIsInodeInlined())
            continue;

         if (isBuddyMirrored)
            dentry.addDentryFeatureFlag(DENTRY_FEATURE_BUDDYMIRRORED);
         else
            dentry.setDentryFeatureFlags(
                  dentry.getDentryFeatureFlags() & ~DENTRY_FEATURE_BUDDYMIRRORED);

         if (!dentry.storeUpdatedDirEntry(entries.getDirEntryPath()))
            return FhgfsOpsErr_INTERNAL;

         EntryInfo ei;

         dentry.getEntryInfo(getID(), 0, &ei);

         std::unique_ptr<FileInode> inode(FileInode::createFromEntryInfo(&ei));
         if (!inode)
            return FhgfsOpsErr_PATHNOTEXISTS;

         inode->setIsBuddyMirrored(isBuddyMirrored);

         if (!inode->updateInodeOnDisk(&ei))
            return FhgfsOpsErr_INTERNAL;
      }
   }

   return FhgfsOpsErr_SUCCESS;
}
