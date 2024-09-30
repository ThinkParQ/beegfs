#ifndef METASTORE_H_
#define METASTORE_H_

#include <common/fsck/FsckDirInode.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/Metadata.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/EntryInfo.h>
#include <common/threading/SafeRWLock.h>
#include <common/threading/Atomics.h>
#include <common/toolkit/FsckTk.h>
#include <common/Common.h>
#include <storage/IncompleteInode.h>
#include <storage/MkFileDetails.h>
#include <session/EntryLock.h>
#include "DirEntry.h"
#include "InodeDirStore.h"
#include "InodeFileStore.h"
#include "MetadataEx.h"
#include "MetaFileHandle.h"


/*
 * This is the main class for all client side posix io operations regarding the meta server.
 * So client side net message will do io via this class.
 */
class MetaStore
{
   public:
      DirInode* referenceDir(const std::string& dirID, const bool isBuddyMirrored,
         const bool forceLoad);
      void releaseDir(const std::string& dirID);
      MetaFileHandle referenceFile(EntryInfo* entryInfo);
      MetaFileHandle referenceLoadedFile(const std::string& parentEntryID,
         bool parentIsBuddyMirrored, const std::string& entryID);
      bool releaseFile(const std::string& parentEntryID, MetaFileHandle& inode);
      bool referenceInode(const std::string& entryID, bool isBuddyMirrored,
         MetaFileHandle& outFileInode, DirInode*& outDirInode);

      FhgfsOpsErr openFile(EntryInfo* entryInfo, unsigned accessFlags, MetaFileHandle& outInode,
         bool checkDisposalFirst = false);
      void closeFile(EntryInfo* entryInfo, MetaFileHandle inode, unsigned accessFlags,
         unsigned* outNumHardlinks, unsigned* outNumRefs);

      FhgfsOpsErr stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData,
         NumNodeID* outParentNodeID = NULL, std::string* outParentEntryID = NULL);

      FhgfsOpsErr setAttr(EntryInfo* entryInfo, int validAttribs, SettableFileAttribs* attribs);
      FhgfsOpsErr incDecLinkCount(EntryInfo* entryInfo, int value);
      FhgfsOpsErr setDirParent(EntryInfo* entryInfo, NumNodeID parentNodeID);

      FhgfsOpsErr mkNewMetaFile(DirInode& dir, MkFileDetails* mkDetails,
         std::unique_ptr<StripePattern> stripePattern, EntryInfo* outEntryInfo,
         FileInodeStoreData* outInodeData);

      FhgfsOpsErr makeDirInode(DirInode& inode);
      FhgfsOpsErr makeDirInode(DirInode& inode, const CharVector& defaultACLXAttr,
         const CharVector& accessACLXAttr);
      FhgfsOpsErr removeDirInode(const std::string& entryID, bool isBuddyMirrored);
      FhgfsOpsErr unlinkInode(EntryInfo* entryInfo, std::unique_ptr<FileInode>* outInode);
      FhgfsOpsErr fsckUnlinkFileInode(const std::string& entryID, bool isBuddyMirrored);
      FhgfsOpsErr unlinkFile(DirInode& dir, const std::string& fileName,
         EntryInfo* outEntryInfo, std::unique_ptr<FileInode>* outInode);
      FhgfsOpsErr unlinkFileInode(EntryInfo* delFileInfo, std::unique_ptr<FileInode>* outInode);

      FhgfsOpsErr unlinkInodeLater(EntryInfo* entryInfo, bool wasInlined);

      FhgfsOpsErr renameInSameDir(DirInode& parentDir, const std::string& fromName,
         const std::string& toName, std::unique_ptr<FileInode>* outUnlinkInode,
         DirEntry*& outOverWrittenEntry, bool& outUnlinkedWasInlined);

      FhgfsOpsErr moveRemoteFileInsert(EntryInfo* fromFileInfo, DirInode& toParent,
            const std::string& newEntryName, const char* buf, uint32_t bufLen,
            std::unique_ptr<FileInode>* outUnlinkedInode, EntryInfo* overWriteInfo, EntryInfo& newFileInfo);

      FhgfsOpsErr moveRemoteFileBegin(DirInode& dir, EntryInfo* entryInfo, char* buf, size_t bufLen,
         size_t* outUsedBufLen);
      void moveRemoteFileComplete(DirInode& dir, const std::string& entryID);

      FhgfsOpsErr getAllInodesIncremental(unsigned hashDirNum, int64_t lastOffset,
          unsigned maxOutInodes, FsckDirInodeList* outDirInodes, FsckFileInodeList* outFileInodes,
          int64_t* outNewOffset, bool isBuddyMirrored);

      FhgfsOpsErr getAllEntryIDFilesIncremental(unsigned firstLevelhashDirNum,
         unsigned secondLevelhashDirNum, int64_t lastOffset, unsigned maxOutEntries,
         StringList* outEntryIDFiles, int64_t* outNewOffset, bool buddyMirrored);

      void getReferenceStats(size_t* numReferencedDirs, size_t* numReferencedFiles);
      void getCacheStats(size_t* numCachedDirs);

      bool cacheSweepAsync();

      FhgfsOpsErr insertDisposableFile(FileInode* inode);

      std::pair<FhgfsOpsErr, bool>  getEntryData(DirInode *dirInode, const std::string& entryName,
         EntryInfo* outInfo, FileInodeStoreData* outInodeMetaData);
      FhgfsOpsErr getEntryData(EntryInfo* inEntryInfo, FileInodeStoreData* outInodeMetaData);

      FhgfsOpsErr linkInSameDir(DirInode& parentDir, EntryInfo* fromFileInfo,
         const std::string& fromName, const std::string& toName);

      FhgfsOpsErr makeNewHardlink(EntryInfo* fromFileInfo);
      FhgfsOpsErr verifyAndMoveFileInode(DirInode& parentDir, EntryInfo* fileInfo,
         FileInodeMode moveMode);
      FhgfsOpsErr checkAndRepairDupFileInode(DirInode& parentDir, EntryInfo* entryInfo);

      FhgfsOpsErr getRawMetadata(const Path& path, CharVector& contents);
      std::pair<FhgfsOpsErr, IncompleteInode> beginResyncFor(const Path& path, bool isDirectory);
      FhgfsOpsErr unlinkRawMetadata(const Path& path);

      void invalidateMirroredDirInodes();

   private:
      InodeDirStore dirStore;

      /* We need to avoid to use that one, as it is a global store, with possible lots of entries.
       * So access to the map is slow and inserting entries blocks the entire MetaStore */
      InodeFileStore fileStore;

      RWLock rwlock; /* note: this is mostly not used as a read/write-lock but rather a shared/excl
         lock (because we're not really modifying anyting directly) - especially relevant for the
         mutliple dirStore locking dual-move methods */

      FhgfsOpsErr isFileUnlinkable(DirInode& subDir, EntryInfo* entryInfo);

      FhgfsOpsErr mkMetaFileUnlocked(DirInode& dir, const std::string& entryName,
         EntryInfo* entryInfo, FileInode* inode);

      FhgfsOpsErr unlinkInodeUnlocked(EntryInfo* entryInfo, DirInode* subDir,
         std::unique_ptr<FileInode>* outInode);
      FhgfsOpsErr unlinkInodeLaterUnlocked(EntryInfo* entryInfo, bool wasInlined);

      FhgfsOpsErr unlinkFileUnlocked(DirInode& subdir, const std::string& fileName,
         std::unique_ptr<FileInode>* outInode, EntryInfo* outEntryInfo, bool& outWasInlined);

      FhgfsOpsErr unlinkDirEntryWithInlinedInodeUnlocked(const std::string& entryName,
            DirInode& subDir, DirEntry* dirEntry, unsigned unlinkTypeFlags,
            std::unique_ptr<FileInode>* outInode);
      FhgfsOpsErr unlinkDentryAndInodeUnlocked(const std::string& fileName, DirInode& subdir,
         DirEntry* dirEntry, unsigned unlinkTypeFlags, std::unique_ptr<FileInode>* outInode);

      FhgfsOpsErr unlinkOverwrittenEntry(DirInode& parentDir, DirEntry* overWrittenEntry,
         std::unique_ptr<FileInode>* outInode);
      FhgfsOpsErr unlinkOverwrittenEntryUnlocked(DirInode& parentDir, DirEntry* overWrittenEntry,
         std::unique_ptr<FileInode>* outInode);


      DirInode* referenceDirUnlocked(const std::string& dirID, bool isBuddyMirrored,
         bool forceLoad);
      void releaseDirUnlocked(const std::string& dirID);
      MetaFileHandle referenceFileUnlocked(EntryInfo* entryInfo);
      MetaFileHandle referenceFileUnlocked(DirInode& subDir, EntryInfo* entryInfo);
      MetaFileHandle referenceLoadedFileUnlocked(const std::string& parentEntryID,
         bool isBuddyMirrored, const std::string& entryID);
      MetaFileHandle referenceLoadedFileUnlocked(DirInode& subDir, const std::string& entryID);
      bool releaseFileUnlocked(const std::string& parentEntryID, MetaFileHandle& inode);
      bool releaseFileUnlocked(DirInode& subDir, MetaFileHandle& inode);

      MetaFileHandle tryReferenceFileWriteLocked(EntryInfo* entryInfo);
      FhgfsOpsErr tryOpenFileWriteLocked(EntryInfo* entryInfo, unsigned accessFlags, MetaFileHandle& outInode);

      bool moveReferenceToMetaFileStoreUnlocked(const std::string& parentEntryID,
         bool parentIsBuddyMirrored, const std::string& entryID);

      FhgfsOpsErr performRenameEntryInSameDir(DirInode& dir, const std::string& fromName,
         const std::string& toName, DirEntry** outOverwrittenEntry);
      FhgfsOpsErr checkRenameOverwrite(EntryInfo* fromEntry, EntryInfo* overWriteEntry,
         bool& outIsSameInode);

      FhgfsOpsErr setAttrUnlocked(EntryInfo* entryInfo, int validAttribs,
         SettableFileAttribs* attribs);
      FhgfsOpsErr incDecLinkCountUnlocked(EntryInfo* entryInfo, int value);

      FhgfsOpsErr verifyAndMoveFileInodeUnlocked(DirInode& parentDir, EntryInfo* fileInfo,
         FileInodeMode moveMode);
      FhgfsOpsErr deinlineFileInode(DirInode& parentDir, EntryInfo* entryInfo,
         DirEntry& dentry, const std::string& dirEntryPath);
      FhgfsOpsErr reinlineFileInode(DirInode& parentDir, EntryInfo* entryInfo,
         DirEntry& dentry, const std::string& dirEntryPath);
   public:
      // getters & setters

      // inliners

};


#endif /* METASTORE_H_ */
