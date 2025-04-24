#pragma once

#include <common/net/message/NetMessage.h>

class MoveFileInodeRespMsg : public NetMessageSerdes<MoveFileInodeRespMsg>
{
   public:
      MoveFileInodeRespMsg(FhgfsOpsErr result, unsigned linkCount) :
         BaseType(NETMSGTYPE_MoveFileInodeResp)
      {
         this->result = result;
         this->linkCount = linkCount;
      }

      /**
       * Constructor for deserialization only
       */
      MoveFileInodeRespMsg() : BaseType(NETMSGTYPE_MoveFileInodeResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->linkCount;
      }

      FhgfsOpsErr getResult() const
      {
         return this->result;
      }

      unsigned getHardlinkCount() const
      {
         return this->linkCount;
      }

   private:
      FhgfsOpsErr result;
      unsigned linkCount;
};
