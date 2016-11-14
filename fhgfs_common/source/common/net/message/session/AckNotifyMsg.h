#ifndef COMMON_ACKNOTIFYMSG
#define COMMON_ACKNOTIFYMSG

#include <common/net/message/NetMessage.h>

class AckNotifiyMsg : public MirroredMessageBase<AckNotifiyMsg>
{
   public:
      AckNotifiyMsg(): BaseType(NETMSGTYPE_AckNotify) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
      }

      bool supportsMirroring() const override { return true; }
};

#endif
