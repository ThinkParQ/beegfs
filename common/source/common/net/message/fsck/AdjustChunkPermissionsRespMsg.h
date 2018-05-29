#ifndef ADJUSTCHUNKPERMISSIONSRESPMSG_H
#define ADJUSTCHUNKPERMISSIONSRESPMSG_H

#include <common/net/message/NetMessage.h>

class AdjustChunkPermissionsRespMsg : public NetMessageSerdes<AdjustChunkPermissionsRespMsg>
{
   public:
      AdjustChunkPermissionsRespMsg(unsigned count, std::string& currentContDirID,
         int64_t newHashDirOffset, int64_t newContDirOffset, unsigned errorCount) :
            BaseType(NETMSGTYPE_AdjustChunkPermissionsResp)
      {
         this->count = count;
         this->currentContDirID = currentContDirID.c_str();
         this->currentContDirIDLen = currentContDirID.length();
         this->newHashDirOffset = newHashDirOffset;
         this->newContDirOffset = newContDirOffset;
         this->errorCount = errorCount;
      }

      AdjustChunkPermissionsRespMsg() : BaseType(NETMSGTYPE_AdjustChunkPermissionsResp)
      {
      }

   private:
      const char* currentContDirID;
      unsigned currentContDirIDLen;
      uint32_t count;
      int64_t newHashDirOffset;
      int64_t newContDirOffset;
      uint32_t errorCount;

   public:
      // getters & setters
      unsigned getCount() const
      {
         return count;
      }

      int64_t getNewHashDirOffset() const
      {
         return newHashDirOffset;
      }

      int64_t getNewContDirOffset() const
      {
         return newContDirOffset;
      }

      std::string getCurrentContDirID() const
      {
         return currentContDirID;
      }

      bool getErrorCount() const
      {
         return errorCount;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::rawString(obj->currentContDirID, obj->currentContDirIDLen, 4)
            % obj->count
            % obj->newHashDirOffset
            % obj->newContDirOffset
            % obj->errorCount;
      }
};


#endif /*ADJUSTCHUNKPERMISSIONSRESPMSG_H*/
