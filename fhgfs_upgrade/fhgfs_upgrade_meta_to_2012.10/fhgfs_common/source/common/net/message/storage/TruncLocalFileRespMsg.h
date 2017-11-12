#ifndef TRUNCLOCALFILERESPMSG_H_
#define TRUNCLOCALFILERESPMSG_H_

#include <common/net/message/NetMessage.h>


class TruncLocalFileRespMsg : public NetMessage
{
   public:
      TruncLocalFileRespMsg(int result, int64_t filesize, int64_t allocedBlocks,
         int64_t modificationTimeSecs, int64_t lastAccessTimeSecs, uint64_t storageVersion) :
         NetMessage(NETMSGTYPE_TruncLocalFileResp)
      {
         this->result = result;
         this->allocedBlocks = allocedBlocks;
         this->filesize = filesize;
         this->modificationTimeSecs = modificationTimeSecs;
         this->lastAccessTimeSecs = lastAccessTimeSecs;
         this->storageVersion = storageVersion;
      }

      /**
       * For deserialization only!
       */
      TruncLocalFileRespMsg() : NetMessage(NETMSGTYPE_TruncLocalFileResp) { }


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
      int result;
      int64_t filesize;
      int64_t allocedBlocks; // number of allocated 512byte blocks (relevant for sparse files)
      int64_t modificationTimeSecs;
      int64_t lastAccessTimeSecs;
      uint64_t storageVersion;


   public:

      // getters & setters
      int getResult()
      {
         return result;
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


#endif /*TRUNCLOCALFILERESPMSG_H_*/
