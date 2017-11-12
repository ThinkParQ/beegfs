#ifndef REMOVEBUDDYGROUPRESPMSG_H_
#define REMOVEBUDDYGROUPRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>

class RemoveBuddyGroupRespMsg : public NetMessageSerdes<RemoveBuddyGroupRespMsg>
{
   public:
      RemoveBuddyGroupRespMsg(FhgfsOpsErr result):
         BaseType(NETMSGTYPE_RemoveBuddyGroupResp), result(result)
      {
      }

      RemoveBuddyGroupRespMsg() : BaseType(NETMSGTYPE_RemoveBuddyGroupResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->result;
      }

   private:
      FhgfsOpsErr result;

   public:
      FhgfsOpsErr getResult() const { return result; }
};

#endif
