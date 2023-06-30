#ifndef FSCKFILEINODE_H
#define FSCKFILEINODE_H

#include <common/Common.h>
#include <common/toolkit/FsckTk.h>
#include <common/storage/PathInfo.h>
#include <common/storage/StatData.h>
#include <common/toolkit/serialization/Serialization.h>

class FsckFileInode;

typedef std::list<FsckFileInode> FsckFileInodeList;
typedef FsckFileInodeList::iterator FsckFileInodeListIter;

class FsckFileInode
{
      friend class TestDatabase;

   public:
      FsckFileInode(const std::string& id, const std::string& parentDirID, NumNodeID parentNodeID,
         const PathInfo& pathInfo, unsigned userID, unsigned groupID, int64_t fileSize,
         unsigned numHardLinks, uint64_t usedBlocks, const UInt16Vector& stripeTargets,
         FsckStripePatternType stripePatternType, unsigned chunkSize, NumNodeID saveNodeID,
         uint64_t saveInode, int32_t saveDevice, bool isInlined, bool isBuddyMirrored,
         bool readable, bool isMismirrored)
      {
         SettableFileAttribs settableFileAttribs;

         settableFileAttribs.userID = userID;
         settableFileAttribs.groupID = groupID;

         StatData statData(fileSize, &settableFileAttribs, 0, 0, numHardLinks, 0);

         initialize(id, parentDirID, parentNodeID, pathInfo, &statData, usedBlocks, stripeTargets,
            stripePatternType, chunkSize, saveNodeID, saveInode, saveDevice, isInlined,
            isBuddyMirrored, readable, isMismirrored);
      }

      FsckFileInode(const std::string& id, const std::string& parentDirID, NumNodeID parentNodeID,
         const PathInfo& pathInfo, StatData* statData, uint64_t usedBlocks,
         const UInt16Vector& stripeTargets, FsckStripePatternType stripePatternType,
         unsigned chunkSize, NumNodeID saveNodeID, uint64_t saveInode, int32_t saveDevice,
         bool isInlined, bool isBuddyMirrored, bool readable, bool isMismirrored)
      {
         initialize(id, parentDirID, parentNodeID, pathInfo, statData, usedBlocks, stripeTargets,
            stripePatternType, chunkSize, saveNodeID, saveInode, saveDevice, isInlined,
            isBuddyMirrored, readable, isMismirrored);
      }

      FsckFileInode() = default;

   private:
      std::string id; // filesystem-wide unique string
      std::string parentDirID;
      NumNodeID parentNodeID;

      PathInfo pathInfo;

      uint32_t userID;
      uint32_t groupID;

      uint64_t usedBlocks; // 512byte-blocks
      int64_t fileSize; // in byte
      uint32_t numHardLinks;

      UInt16Vector stripeTargets;
      FsckStripePatternType stripePatternType;
      uint32_t chunkSize;

      NumNodeID saveNodeID; // id of the node, where this inode is saved on

      bool isInlined;
      bool isBuddyMirrored;
      bool readable;
      bool isMismirrored;

      uint64_t saveInode;
      int32_t saveDevice;

      void initialize(const std::string& id, const std::string& parentDirID, NumNodeID parentNodeID,
         const PathInfo& pathInfo, StatData* statData, uint64_t usedBlocks,
         const UInt16Vector& stripeTargets, FsckStripePatternType stripePatternType,
         unsigned chunkSize, NumNodeID saveNodeID, uint64_t saveInode, int32_t saveDevice,
         bool isInlined, bool isBuddyMirrored, bool readable, bool isMismirrored)
      {
         this->id = id;
         this->parentDirID = parentDirID;
         this->parentNodeID = parentNodeID;
         this->pathInfo = pathInfo;

         /* check pathInfo; deserialization from disk does not set origParentEntryID, if file was
          * never moved. On fsck side, we need this set, so we set it to the current parent dir
          * here */
         if (this->pathInfo.getOrigParentEntryID().empty())
            this->pathInfo.setOrigParentEntryID(parentDirID);

         this->userID = statData->getUserID();
         this->groupID = statData->getGroupID();
         this->fileSize = statData->getFileSize();
         this->numHardLinks = statData->getNumHardlinks();
         this->usedBlocks = usedBlocks;
         this->stripeTargets = stripeTargets;
         this->stripePatternType = stripePatternType;
         this->chunkSize = chunkSize;
         this->saveNodeID = saveNodeID;
         this->saveInode = saveInode;
         this->saveDevice = saveDevice;
         this->isInlined = isInlined;
         this->isBuddyMirrored = isBuddyMirrored;
         this->readable = readable;
         this->isMismirrored = isMismirrored;
      }

   public:
      const std::string& getID() const { return id; }

      const std::string& getParentDirID() const { return parentDirID; }

      NumNodeID getParentNodeID() const { return parentNodeID; }

      PathInfo* getPathInfo() { return &(this->pathInfo); }

      unsigned getUserID() const { return this->userID; }

      unsigned getGroupID() const { return this->groupID; }

      int64_t getFileSize() const { return this->fileSize; }
      void setFileSize(int64_t fileSize) { this->fileSize = fileSize; }

      unsigned getNumHardLinks() const { return this->numHardLinks; }
      void setNumHardLinks(unsigned numHardLinks) { this->numHardLinks = numHardLinks; }

      uint64_t getUsedBlocks() const { return this->usedBlocks; }

      const UInt16Vector& getStripeTargets() const { return stripeTargets; }
      void setStripeTargets(const UInt16Vector& targets) { stripeTargets = targets; }

      FsckStripePatternType getStripePatternType() const { return stripePatternType; }

      unsigned getChunkSize() const { return chunkSize; }

      NumNodeID getSaveNodeID() const { return saveNodeID; }

      bool getIsInlined() const { return isInlined; }
      void setIsInlined(bool value) { isInlined = value; }

      bool getIsBuddyMirrored() const { return isBuddyMirrored; }

      bool getReadable() const { return readable; }

      uint64_t getSaveInode() const { return this->saveInode; }

      int32_t getSaveDevice() const { return this->saveDevice; }

      bool getIsMismirrored() const { return isMismirrored; }

      bool operator<(const FsckFileInode& other) const
      {
         return id < other.id;
      }

      bool operator==(const FsckFileInode& other) const
      {
         return id == other.id &&
            parentDirID == other.parentDirID &&
            parentNodeID == other.parentNodeID &&
            userID == other.userID &&
            groupID == other.groupID &&
            fileSize == other.fileSize &&
            numHardLinks == other.numHardLinks &&
            usedBlocks == other.usedBlocks &&
            stripeTargets == other.stripeTargets &&
            stripePatternType == other.stripePatternType &&
            chunkSize == other.chunkSize &&
            saveNodeID == other.saveNodeID &&
            saveInode == other.saveInode &&
            saveDevice == other.saveDevice &&
            isInlined == other.isInlined &&
            isBuddyMirrored == other.isBuddyMirrored &&
            readable == other.readable &&
            isMismirrored == other.isMismirrored;
      }

      bool operator!= (const FsckFileInode& other) const
      {
         return !(*this == other);
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::stringAlign4(obj->id)
            % serdes::stringAlign4(obj->parentDirID)
            % obj->parentNodeID
            % obj->pathInfo
            % obj->userID
            % obj->groupID
            % obj->fileSize
            % obj->numHardLinks
            % obj->usedBlocks
            % obj->stripeTargets
            % serdes::as<int32_t>(obj->stripePatternType)
            % obj->chunkSize
            % obj->saveNodeID
            % obj->saveInode
            % obj->saveDevice
            % obj->isInlined
            % obj->isBuddyMirrored
            % obj->readable
            % obj->isMismirrored;
      }
};

template<>
struct ListSerializationHasLength<FsckFileInode> : boost::false_type {};

#endif /* FSCKFILEINODE_H */
