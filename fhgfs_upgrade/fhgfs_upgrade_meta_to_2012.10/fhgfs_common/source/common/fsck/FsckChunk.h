#ifndef FSCKCHUNK_H_
#define FSCKCHUNK_H_

#include <common/Common.h>

class FsckChunk;

typedef std::list<FsckChunk> FsckChunkList;
typedef FsckChunkList::iterator FsckChunkListIter;

class FsckChunk
{
   friend class TestDatabase;

   public:
      size_t serialize(char* outBuf);
      bool deserialize(const char* buf, size_t bufLen, unsigned* outLen);
      unsigned serialLen();

   private:
      std::string id;
      uint16_t targetID;
      int64_t fileSize; // in byte
      int64_t creationTime; // secs since the epoch
      int64_t modificationTime; // secs since the epoch
      int64_t lastAccessTime; // secs since the epoch

   public:
      /*
       * @param id the chunk id
       * @param targetID the id of the target, on which the chunk is saved
       * @param filesize the size of the chunk in byte
       * @param creationTime creation time in secs since the epoch
       * @param lastAccessTime last access time in secs since the epoch
       * @param modificationTime modification time in secs since the epoch
       * */
      FsckChunk(std::string id, uint16_t targetID, int64_t fileSize, int64_t creationTime,
         int64_t modificationTime, int64_t lastAccessTime) : id(id), targetID(targetID),
         fileSize(fileSize), creationTime(creationTime), modificationTime(modificationTime),
         lastAccessTime(lastAccessTime)
      {
      }

      // only for deserialization!
      FsckChunk()
      {
      }

      std::string getID() const
      {
         return this->id;
      }

      uint16_t getTargetID() const
      {
         return this->targetID;
      }

      int64_t getFileSize() const
      {
         return this->fileSize;
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

      bool operator<(const FsckChunk& other)
      {
         if ( id < other.id )
            return true;
         else
            return false;
      }

      bool operator==(const FsckChunk& other)
      {
         if ( id.compare(other.id) != 0 )
            return false;
         else
         if ( targetID != other.targetID )
            return false;
         else
         if ( fileSize != other.fileSize )
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
            return true;
      }

      bool operator!= (const FsckChunk& other)
      {
         return !(operator==( other ) );
      }

      void update(std::string id, uint16_t targetID, uint64_t fileSize, int64_t creationTime,
         int64_t modificationTime, int64_t lastAccessTime)
      {
         this->id = id;
         this->targetID = targetID;
         this->fileSize = fileSize;
         this->creationTime = creationTime;
         this->modificationTime = modificationTime;
         this->lastAccessTime = lastAccessTime;
      }
};

#endif /* FSCKCHUNK_H_ */
