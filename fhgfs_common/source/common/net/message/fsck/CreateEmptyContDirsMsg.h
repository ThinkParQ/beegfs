#ifndef CREATEEMPTYCONTDIRSMSG_H_
#define CREATEEMPTYCONTDIRSMSG_H_

#include <common/net/message/NetMessage.h>

class CreateEmptyContDirsMsg : public NetMessageSerdes<CreateEmptyContDirsMsg>
{
   public:
      typedef std::tuple<std::string, bool> Item;

      CreateEmptyContDirsMsg(std::vector<Item> items):
         BaseType(NETMSGTYPE_CreateEmptyContDirs), items(std::move(items))
      {
      }

      CreateEmptyContDirsMsg() : BaseType(NETMSGTYPE_CreateEmptyContDirs)
      {
      }

   protected:
      std::vector<Item> items;

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->items;
      }
};

#endif /* CREATEEMPTYCONTDIRSMSG_H_ */
