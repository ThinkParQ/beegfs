#ifndef FSCKFILEINODE_H
#define FSCKFILEINODE_H

#include <common/Common.h>
#include <common/toolkit/FsckTk.h>

class FsckFileInode;

typedef std::list<FsckFileInode> FsckFileInodeList;
typedef FsckFileInodeList::iterator FsckFileInodeListIter;

class FsckFileInode
{
   friend class TestDatabase;

   public:
      FsckFileInode(std::string id, std::string parentDirID, uint16_t parentNodeID, int mode,
         unsigned userID, unsigned groupID, int64_t fileSize, int64_t creationTime,
         int64_t modificationTime, int64_t lastAccessTime, unsigned numHardLinks,
         UInt16Vector& stripeTargets, FsckStripePatternType stripePatternType, unsigned chunkSize,
         uint16_t saveNodeID, bool readable = true) :
         id(id), parentDirID(parentDirID), parentNodeID(parentNodeID), mode(mode),
         userID(userID), groupID(groupID), fileSize(fileSize), creationTime(creationTime),
         modificationTime(modificationTime), lastAccessTime(lastAccessTime),
         numHardLinks(numHardLinks), stripeTargets(stripeTargets),
         stripePatternType(stripePatternType), chunkSize(chunkSize), saveNodeID(saveNodeID),
         readable(readable)
      {
      }

      FsckFileInode(std::string id, std::string parentDirID, uint16_t parentNodeID,
         StatData* statData, UInt16Vector& stripeTargets, FsckStripePatternType stripePatternType,
         unsigned chunkSize, uint16_t saveNodeID, bool readable = true)
      {
         this->id = id;
         this->parentDirID = parentDirID;
         this->parentNodeID = parentNodeID;
         this->mode = statData->getMode();
         this->userID = statData->getUserID();
         this->groupID = statData->getGroupID();
         this->fileSize = statData->getFileSize();
         this->creationTime = statData->getCreationTimeSecs();
         this->modificationTime = statData->getModificationTimeSecs();
         this->lastAccessTime = statData->getLastAccessTimeSecs();
         this->numHardLinks = statData->getNumHardlinks();
         this->stripeTargets = stripeTargets;
         this->stripePatternType = stripePatternType;
         this->chunkSize = chunkSize;
         this->saveNodeID = saveNodeID;
      }

      FsckFileInode()
      {
      }

      size_t serialize(char* outBuf);
      bool deserialize(const char* buf, size_t bufLen, unsigned* outLen);
      unsigned serialLen();

   private:
      std::string id; // filesystem-wide unique string
      std::string parentDirID;
      uint16_t parentNodeID;

      int mode;
      unsigned userID;
      unsigned groupID;

      int64_t fileSize; // in byte
      int64_t creationTime; // secs since the epoch
      int64_t modificationTime; // secs since the epoch
      int64_t lastAccessTime; // secs since the epoch
      unsigned numHardLinks;

      UInt16Vector stripeTargets;
      FsckStripePatternType stripePatternType;
      unsigned chunkSize;

      uint16_t saveNodeID; // id of the node, where this inode is saved on
      bool readable;

   public:
      std::string getID() const
      {
         return id;
      }

      std::string getParentDirID() const
      {
         return parentDirID;
      }

      void setParentDirID(std::string parentDirID)
      {
         this->parentDirID = parentDirID;
      }

      uint16_t getParentNodeID() const
      {
         return parentNodeID;
      }

      void setParentNodeID(uint16_t parentNodeID)
      {
         this->parentNodeID = parentNodeID;
      }

      int getMode() const
      {
         return mode;
      }

      void setMode(int mode)
      {
         this->mode = mode;
      }

      unsigned getUserID() const
      {
         return this->userID;
      }

      void setUserID(unsigned userID)
      {
         this->userID = userID;
      }

      unsigned getGroupID() const
      {
         return this->groupID;
      }

      void setGroupID(unsigned groupID)
      {
         this->groupID = groupID;
      }

      int64_t getFileSize() const
      {
         return this->fileSize;
      }

      void setFileSize(int64_t fileSize)
      {
         this->fileSize = fileSize;
      }

      int64_t getCreationTime() const
      {
         return this->creationTime;
      }

      void setCreationTime(int64_t creationTime)
      {
         this->creationTime = creationTime;
      }

      int64_t getModificationTime() const
      {
         return this->modificationTime;
      }

      void setModificationTime(int64_t modificationTime)
      {
         this->modificationTime = modificationTime;
      }

      int64_t getLastAccessTime() const
      {
         return this->lastAccessTime;
      }

      void setLastAccessTime(int64_t lastAccessTime)
      {
         this->lastAccessTime = lastAccessTime;
      }

      unsigned getNumHardLinks() const
      {
         return this->numHardLinks;
      }

      void setNumHardLinks(unsigned numHardLinks)
      {
         this->numHardLinks = numHardLinks;
      }

      UInt16Vector getStripeTargets() const
      {
         return stripeTargets;
      }

      UInt16Vector* getStripeTargets()
      {
         return &stripeTargets;
      }

      void setStripeTargets(UInt16Vector& stripeTargets)
      {
         this->stripeTargets = stripeTargets;
      }

      FsckStripePatternType getStripePatternType() const
      {
         return stripePatternType;
      }

      void setStripePatternType(FsckStripePatternType stripePatternType)
      {
         this->stripePatternType = stripePatternType;
      }

      unsigned getChunkSize() const
      {
         return chunkSize;
      }

      void setChunkSize(unsigned chunkSize)
      {
         this->chunkSize = chunkSize;
      }

      uint16_t getSaveNodeID() const
      {
         return saveNodeID;
      }

      bool getReadable() const
      {
         return readable;
      }

      void setReadable(bool readable)
      {
         this->readable = readable;
      }

      bool operator<(const FsckFileInode& other)
      {
         if ( id < other.id )
            return true;
         else
            return false;
      }

      bool operator==(const FsckFileInode& other)
      {
         if ( id.compare(other.id) != 0 )
            return false;
         else
         if ( parentDirID.compare(other.parentDirID) != 0 )
            return false;
         else
         if ( parentNodeID != other.parentNodeID )
            return false;
         else
         if ( mode != other.mode )
            return false;
         else
         if ( userID != other.userID )
            return false;
         else
         if ( groupID != other.groupID )
            return false;
         else
         if ( fileSize != other.fileSize )
            return false;
         else
         if ( creationTime != other.creationTime )
            return false;
         else
         if ( modificationTime != other.modificationTime )
            return false;
         else
         if ( lastAccessTime != other.lastAccessTime )
            return false;
         else
         if ( numHardLinks != other.numHardLinks )
            return false;
         else
         if ( stripeTargets != other.stripeTargets )
            return false;
         else
         if ( stripePatternType != other.stripePatternType )
            return false;
         else
         if ( chunkSize != other.chunkSize )
            return false;
         else
         if ( saveNodeID != other.saveNodeID )
            return false;
         else
         if ( readable != other.readable )
            return false;
         else
            return true;
      }

      bool operator!= (const FsckFileInode& other)
      {
         return !(operator==( other ) );
      }

      void update (std::string id, std::string parentDirID, uint16_t parentNodeID, int mode,
         unsigned userID, unsigned groupID, int64_t fileSize, int64_t creationTime,
         int64_t modificationTime, int64_t lastAccessTime, unsigned numHardLinks,
         UInt16Vector& stripeTargets, FsckStripePatternType stripePatternType, unsigned chunkSize,
         uint16_t saveNodeID, bool readable)
      {
         this->id = id;
         this->parentDirID = parentDirID;
         this->parentNodeID = parentNodeID;

         this->mode = mode;
         this->userID = userID;
         this->groupID = groupID;

         this->fileSize = fileSize;
         this->creationTime = creationTime;
         this->modificationTime = modificationTime;
         this->lastAccessTime = lastAccessTime;
         this->numHardLinks = numHardLinks;

         this->stripeTargets = stripeTargets;
         this->stripePatternType = stripePatternType;
         this->chunkSize = chunkSize;

         this->saveNodeID = saveNodeID;
         this->readable = readable;
      }
};

#endif /* FSCKFILEINODE_H */
