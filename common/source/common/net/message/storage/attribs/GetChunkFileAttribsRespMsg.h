#ifndef GETCHUNKFILEATTRIBSRESPMSG_H_
#define GETCHUNKFILEATTRIBSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

class GetChunkFileAttribsRespMsg : public NetMessageSerdes<GetChunkFileAttribsRespMsg>
{
   public:
      GetChunkFileAttribsRespMsg(FhgfsOpsErr result, int64_t size, int64_t allocedBlocks,
         int64_t modificationTimeSecs, int64_t lastAccessTimeSecs, uint64_t storageVersion) :
         BaseType(NETMSGTYPE_GetChunkFileAttribsResp)
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
      GetChunkFileAttribsRespMsg() : BaseType(NETMSGTYPE_GetChunkFileAttribsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->size
            % obj->allocedBlocks
            % obj->modificationTimeSecs
            % obj->lastAccessTimeSecs
            % obj->storageVersion
            % obj->result;
      }

   private:
      int32_t result;
      int64_t size;
      int64_t allocedBlocks; // allocated 512byte blocks ("!= size/512" for sparse files)
      int64_t modificationTimeSecs;
      int64_t lastAccessTimeSecs;
      uint64_t storageVersion;


   public:
      // getters & setters
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)result;
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
