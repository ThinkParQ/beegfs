#ifndef FSCKCHUNK_H_
#define FSCKCHUNK_H_

#include <common/Common.h>
#include <common/storage/Path.h>
#include <common/storage/PathInfo.h>
#include <common/toolkit/serialization/Serialization.h>

#include <iostream>

class FsckChunk;

typedef std::list<FsckChunk> FsckChunkList;
typedef FsckChunkList::iterator FsckChunkListIter;

class FsckChunk
{
   friend class TestDatabase;

   public:
      /*
       * @param id the chunk id
       * @param targetID the id of the target, on which the chunk is saved
       * @param savedPath the path, where the chunk was found; relative to storeStorageDirectory
       * @param filesize the size of the chunk in byte
       * @param usedBlocks the actual used blocks on the FS
       * @param creationTime creation time in secs since the epoch
       * @param lastAccessTime last access time in secs since the epoch
       * @param modificationTime modification time in secs since the epoch
       * @param userID
       * @param groupID
       * @param buddyGroupID the buddyGroupID this chunk belongs to
       */
      FsckChunk(const std::string& id, uint16_t targetID, const Path& savedPath, int64_t fileSize,
         uint64_t usedBlocks, int64_t creationTime, int64_t modificationTime, int64_t lastAccessTime,
         unsigned userID, unsigned groupID, uint16_t buddyGroupID) :
         id(id), targetID(targetID), savedPath(savedPath), fileSize(fileSize),
         usedBlocks(usedBlocks), creationTime(creationTime),  modificationTime(modificationTime),
         lastAccessTime(lastAccessTime), userID(userID), groupID(groupID),
         buddyGroupID(buddyGroupID)
      {
      }

      /*
       * @param id the chunk id
       * @param targetID the id of the target, on which the chunk is saved
       * @param savedPath the path, where the chunk was found; relative to storeStorageDirectory
       * @param filesize the size of the chunk in byte
       * @param usedBlocks the actual used blocks on the FS
       * @param creationTime creation time in secs since the epoch
       * @param lastAccessTime last access time in secs since the epoch
       * @param modificationTime modification time in secs since the epoch
       * @param userID
       * @param groupID
       */
      FsckChunk(const std::string& id, uint16_t targetID, const Path& savedPath,
         int64_t fileSize, int64_t usedBlocks, int64_t creationTime, int64_t modificationTime,
         int64_t lastAccessTime, unsigned userID, unsigned groupID) :
         id(id), targetID(targetID), savedPath(savedPath), fileSize(fileSize),
         usedBlocks(usedBlocks), creationTime(creationTime), modificationTime(modificationTime),
         lastAccessTime(lastAccessTime), userID(userID), groupID(groupID), buddyGroupID(0)
      {
      }

      // only for deserialization!
      FsckChunk()
      {
      }

   private:
      std::string id;
      uint16_t targetID;
      Path savedPath; // the path, where the chunk was found; relative to storeStorageDirectory
      int64_t fileSize; // in byte
      uint64_t usedBlocks;
      int64_t creationTime; // secs since the epoch
      int64_t modificationTime; // secs since the epoch
      int64_t lastAccessTime; // secs since the epoch
      uint32_t userID;
      uint32_t groupID;
      uint16_t buddyGroupID; // if this is a buddy mirrored chunk, this field saves the
                             // buddyGroupID

   public:
      const std::string& getID() const
      {
         return this->id;
      }

      uint16_t getTargetID() const
      {
         return this->targetID;
      }

      Path* getSavedPath()
      {
         return &(this->savedPath);
      }

      void setSavedPath(const Path& path)
      {
         this->savedPath = path;
      }

      int64_t getFileSize() const
      {
         return this->fileSize;
      }

      int64_t getUsedBlocks() const
      {
         return this->usedBlocks;
      }

      int64_t getCreationTime() const
      {
         return this->creationTime;
      }

      int64_t getModificationTime() const
      {
         return this->modificationTime;
      }

      int64_t getLastAccessTime() const
      {
         return this->lastAccessTime;
      }

      unsigned getUserID() const
      {
         return this->userID;
      }

      void setUserID(const unsigned userID)
      {
         this->userID = userID;
      }

      unsigned getGroupID() const
      {
         return this->groupID;
      }

      void setGroupID(const unsigned groupID)
      {
         this->groupID = groupID;
      }

      uint16_t getBuddyGroupID() const
      {
         return this->buddyGroupID;
      }

      bool operator<(const FsckChunk& other) const
      {
         if ( id < other.id )
            return true;
         else
            return false;
      }

      bool operator==(const FsckChunk& other) const
      {
         if ( id != other.id )
            return false;
         else
         if ( targetID != other.targetID )
            return false;
         else
         if ( savedPath != other.savedPath )
            return false;
         else
         if ( fileSize != other.fileSize )
            return false;
         else
         if ( usedBlocks != other.usedBlocks )
            return false;
         else
         if ( creationTime != other.creationTime )
            return false;
         else
         if ( lastAccessTime != other.lastAccessTime )
            return false;
         else
         if ( modificationTime != other.modificationTime )
            return false;
         else
         if ( userID != other.userID )
            return false;
         else
         if ( groupID != other.groupID )
            return false;
         else
         if ( buddyGroupID != other.buddyGroupID )
            return false;
         else
            return true;
      }

      bool operator!= (const FsckChunk& other) const
      {
         return !(operator==( other ) );
      }

      void print()
      {
         std::cout << "id: " << id << '\n'
            << "targetID: " << targetID << '\n'
            << "savedPath: " << savedPath.str() << '\n'
            << "fileSize: " << fileSize << '\n'
            << "usedBlocks: " << usedBlocks << '\n'
            << "creationTime: " << creationTime << '\n'
            << "modificationTime: " << modificationTime << '\n'
            << "lastAccessTime: " << lastAccessTime << '\n'
            << "userID: " << userID << '\n'
            << "buddyGroupID: " << buddyGroupID << std::endl;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::stringAlign4(obj->id)
            % obj->targetID
            % obj->savedPath
            % obj->fileSize
            % obj->usedBlocks
            % obj->creationTime
            % obj->modificationTime
            % obj->lastAccessTime
            % obj->userID
            % obj->groupID
            % obj->buddyGroupID;
      }
};

template<>
struct ListSerializationHasLength<FsckChunk> : boost::false_type {};

#endif /* FSCKCHUNK_H_ */
