#pragma once

#include <common/net/message/NetMessage.h>

class UnlinkLocalFileInodeRespMsg : public NetMessageSerdes<UnlinkLocalFileInodeRespMsg>
{
   public:
      UnlinkLocalFileInodeRespMsg(FhgfsOpsErr result, unsigned linkCount) :
         BaseType(NETMSGTYPE_UnlinkLocalFileInodeResp)
      {
         this->result = result;
         this->preUnlinkHardlinkCount = linkCount;
      }

      /**
       * Constructor for deserialization only.
       */
      UnlinkLocalFileInodeRespMsg() : BaseType(NETMSGTYPE_UnlinkLocalFileInodeResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->preUnlinkHardlinkCount;
      }

      // getters & setters
      FhgfsOpsErr getResult() const
      {
         return this->result;
      }

      unsigned getPreUnlinkHardlinkCount() const
      {
         return this->preUnlinkHardlinkCount;
      }

      private:
         FhgfsOpsErr result;
         unsigned preUnlinkHardlinkCount;
};
