#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/ListTk.h>

class RemoveInodesMsg : public NetMessageSerdes<RemoveInodesMsg>
{
   public:
      typedef std::tuple<std::string, DirEntryType, bool> Item;

      RemoveInodesMsg(std::vector<Item> items):
         BaseType(NETMSGTYPE_RemoveInodes), items(std::move(items))
      {
      }

      RemoveInodesMsg() : BaseType(NETMSGTYPE_RemoveInodes)
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


