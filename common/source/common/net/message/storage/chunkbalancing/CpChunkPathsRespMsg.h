#pragma once

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>

class CpChunkPathsRespMsg : public NetMessageSerdes<CpChunkPathsRespMsg>
{
   public:
      CpChunkPathsRespMsg(FhgfsOpsErr result ) : BaseType(NETMSGTYPE_CpChunkPathsResp)
      {
         this->result = static_cast<int32_t>(result);
      }

      CpChunkPathsRespMsg() : BaseType(NETMSGTYPE_CpChunkPathsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->result;
      }

   private:
      // for serialization
      int32_t result; // FhgfsOpsErr

      // for deserialization
      struct {
         int32_t result;
      } parsed;

   public:
       // getters & setters
      FhgfsOpsErr getResult()
      {
         return static_cast<FhgfsOpsErr>(result);
      }
};


