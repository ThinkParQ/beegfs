#ifndef CLOSECHUNKFILERESPMSG_H_
#define CLOSECHUNKFILERESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageErrors.h>


class CloseChunkFileRespMsg : public NetMessage
{
   public:
      CloseChunkFileRespMsg(FhgfsOpsErr result, int64_t filesize, int64_t allocedBlocks,
         int64_t modificationTimeSecs, int64_t lastAccessTimeSecs, uint64_t storageVersion) :
         NetMessage(NETMSGTYPE_CloseChunkFileResp)
      {
         this->result = (int)result;
         this->filesize = filesize;
         this->allocedBlocks = allocedBlocks;
         this->modificationTimeSecs = modificationTimeSecs;
         this->lastAccessTimeSecs = lastAccessTimeSecs;
         this->storageVersion = storageVersion;
      }

      /**
       * For deserialization only!
       */
      CloseChunkFileRespMsg() : NetMessage(NETMSGTYPE_CloseChunkFileResp) { }


   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt64() + // filesize
            Serialization::serialLenInt64() + // allocedBlocks
            Serialization::serialLenInt64() + // modificationTimeSecs
            Serialization::serialLenInt64() + // lastAccessTimeSecs
            Serialization::serialLenUInt64() + // storageVersion
            Serialization::serialLenInt(); // result
      }


   private:
      int result; // FhgfsOpsErr_...
      int64_t filesize;
      int64_t allocedBlocks; // number of allocated 512byte blocks (relevant for sparse files)
      int64_t modificationTimeSecs;
      int64_t lastAccessTimeSecs;
      uint64_t storageVersion;


   public:
   
      // getters & setters
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)result;
      }
      
      int64_t getFileSize()
      {
         return filesize;
      }

      int64_t getAllocedBlocks()
      {
         return allocedBlocks;
      }

      int64_t getModificationTimeSecs()
      {
         return modificationTimeSecs;
      }

      int64_t getLastAccessTimeSecs()
      {
         return lastAccessTimeSecs;
      }
      
      uint64_t getStorageVersion()
      {
         return storageVersion;
      }

};

#endif /*CLOSECHUNKFILERESPMSG_H_*/
