#ifndef INODEFILESTORE_H_
#define INODEFILESTORE_H_

#include <common/toolkit/ObjectReferencer.h>
#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/MetadataTk.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include "FileInode.h"


typedef ObjectReferencer<FileInode*> FileInodeReferencer;
typedef std::map<std::string, FileInodeReferencer*> InodeMap;
typedef InodeMap::iterator InodeMapIter;
typedef InodeMap::const_iterator InodeMapCIter;
typedef InodeMap::value_type InodeMapVal;

/**
 * Layer in between our inodes and the data on the underlying file system. So we read/write from/to
 * underlying inodes and this class is to do this corresponding data access.
 * This object is used for all file types, for example regular files, but NOT directories.
 */
class InodeFileStore
{
   friend class DirInode;
   friend class MetaStore;

   public:
      InodeFileStore() {}
      ~InodeFileStore()
      {
         this->clearStoreUnlocked();
      }

      bool isInStore(const std::string& fileID);
      FileInode* referenceFileInode(EntryInfo* entryInfo, bool loadFromDisk);
      FileInode* referenceLoadedFile(const std::string& entryID);
      bool releaseFileInode(FileInode* inode);
      FhgfsOpsErr unlinkFileInode(EntryInfo* entryInfo, std::unique_ptr<FileInode>* outInode);
      void unlinkAllFiles();

      FhgfsOpsErr moveRemoteBegin(EntryInfo* entryInfo, char* buf, size_t bufLen,
         size_t* outUsedBufLen);
      void moveRemoteComplete(const std::string& entryID);

      size_t getSize();

      bool closeFile(EntryInfo* entryInfo, FileInode* inode, unsigned accessFlags,
         unsigned* outNumHardlinks, unsigned* outNumRefs);
      FhgfsOpsErr openFile(EntryInfo* entryInfo, unsigned accessFlags,
         FileInode*& outInode, bool loadFromDisk);

      FhgfsOpsErr stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData);
      FhgfsOpsErr setAttr(EntryInfo* entryInfo, int validAttribs, SettableFileAttribs* attribs);

      FhgfsOpsErr isUnlinkable(EntryInfo* entryInfo);

   private:
      InodeMap inodes;

      RWLock rwlock;

      unsigned decreaseInodeRefCountUnlocked(InodeMapIter& iter);
      FileInode* referenceFileInodeUnlocked(EntryInfo* entryInfo, bool loadFromDisk);
      FhgfsOpsErr getUnreferencedInodeUnlocked(EntryInfo* entryInfo, FileInode*& outInode);
      void deleteUnreferencedInodeUnlocked(const std::string& entryID);

      FhgfsOpsErr isUnlinkableUnlocked(EntryInfo* entryInfo);

      FhgfsOpsErr unlinkFileInodeUnlocked(EntryInfo* entryInfo,
            std::unique_ptr<FileInode>* outInode);

      bool loadAndInsertFileInodeUnlocked(EntryInfo* entryInfo, InodeMapIter& newElemIter);
      bool insertReferencer(std::string entryID, FileInodeReferencer* fileRefer);

      FileInodeReferencer* getReferencerAndDeleteFromMap(const std::string& fileID);

      void clearStoreUnlocked();

      FileInode* referenceFileInodeMapIterUnlocked(InodeMapIter& iter);

      FhgfsOpsErr incDecLinkCount(FileInode& inode, EntryInfo* entryInfo, int value);

   public:

      // inliners

      FhgfsOpsErr incLinkCount(FileInode& inode, EntryInfo* entryInfo)
      {
         return incDecLinkCount(inode, entryInfo, 1);
      }

      FhgfsOpsErr decLinkCount(FileInode& inode, EntryInfo* entryInfo)
      {
         return incDecLinkCount(inode, entryInfo, -1);
      }


   private:

      // inliners

      /**
       * Create an unreferenced file inode from an existing inode on disk disk.
       */
      FileInode* createUnreferencedInodeUnlocked(EntryInfo* entryInfo)
      {
         return FileInode::createFromEntryInfo(entryInfo);
      }

};

#endif /*INODEFILESTORE_H_*/
