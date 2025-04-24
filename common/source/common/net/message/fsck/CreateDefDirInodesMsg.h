#pragma once

#include <common/net/message/NetMessage.h>

class CreateDefDirInodesMsg : public NetMessageSerdes<CreateDefDirInodesMsg>
{
   public:
      typedef std::tuple<std::string, bool> Item;

      CreateDefDirInodesMsg(std::vector<Item> items):
         BaseType(NETMSGTYPE_CreateDefDirInodes), items(std::move(items))
      {
      }

      CreateDefDirInodesMsg() : BaseType(NETMSGTYPE_CreateDefDirInodes)
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

