#ifndef GETCHUNKFILEATTRIBSRESPMSG_H_
#define GETCHUNKFILEATTRIBSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class GetChunkFileAttribsRespMsg : public NetMessage
{
   public:
      GetChunkFileAttribsRespMsg(int result, int64_t size, int64_t allocedBlocks,
         int64_t modificationTimeSecs, int64_t lastAccessTimeSecs, uint64_t storageVersion) :
         NetMessage(NETMSGTYPE_GetChunkFileAttribsResp)
      {
         this->result = result;
         this->size = size;
         this->allocedBlocks = allocedBlocks;
         this->modificationTimeSecs = modificationTimeSecs;
         this->lastAccessTimeSecs = lastAccessTimeSecs;
         this->storageVersion = storageVersion;
      }

      /**
       * For deserialization only!
       */
      GetChunkFileAttribsRespMsg() : NetMessage(NETMSGTYPE_GetChunkFileAttribsResp)
      {
      }
   

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt64() + // size
            Serialization::serialLenInt64() + // allocedBlocks
            Serialization::serialLenInt64() + // modificationTimeSecs
            Serialization::serialLenInt64() + // lastAccessTimeSecs
            Serialization::serialLenUInt64() + // storageVersion
            Serialization::serialLenInt(); // result
      }
      

   private:
      int result;
      int64_t size;
      int64_t allocedBlocks; // allocated 512byte blocks ("!= size/512" for sparse files)
      int64_t modificationTimeSecs;
      int64_t lastAccessTimeSecs;
      uint64_t storageVersion;


   public:
      // getters & setters
      int getResult()
      {
         return result;
      }

      int64_t getSize()
      {
         return size;
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

#endif /*GETCHUNKFILEATTRIBSRESPMSG_H_*/
