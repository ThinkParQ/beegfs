#ifndef REMOVEINODESRESPMSG_H
#define REMOVEINODESRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/ListTk.h>

class RemoveInodesRespMsg : public NetMessageSerdes<RemoveInodesRespMsg>
{
   public:
      RemoveInodesRespMsg(StringList failedEntryIDList) :
         BaseType(NETMSGTYPE_RemoveInodesResp), failedEntryIDList(std::move(failedEntryIDList))
      {
      }

      RemoveInodesRespMsg() : BaseType(NETMSGTYPE_RemoveInodesResp)
      {
      }

   private:
      StringList failedEntryIDList;

   public:
      StringList releaseFailedEntryIDList() { return failedEntryIDList; }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->failedEntryIDList;
      }
};


#endif /*REMOVEINODESRESPMSG_H*/
