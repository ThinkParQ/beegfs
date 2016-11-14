#ifndef GETFILEVERSIONRESPMSG_H
#define GETFILEVERSIONRESPMSG_H

#include <common/net/message/NetMessage.h>

class GetFileVersionRespMsg : public NetMessageSerdes<GetFileVersionRespMsg>
{
   public:
      GetFileVersionRespMsg(FhgfsOpsErr result, uint64_t version) :
         BaseType(NETMSGTYPE_GetFileVersionResp),
         result(result), version(version)
      {
      }

      GetFileVersionRespMsg() : BaseType(NETMSGTYPE_GetFileVersionResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->version;
      }

      uint64_t getVersion() const { return version; }

   private:
      FhgfsOpsErr result;
      uint64_t version;
};


#endif
