#ifndef CHECKANDREPAIRDUPINODERESPMSG_H
#define CHECKANDREPAIRDUPINODERESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/ListTk.h>

class CheckAndRepairDupInodeRespMsg : public NetMessageSerdes<CheckAndRepairDupInodeRespMsg>
{
   public:
      CheckAndRepairDupInodeRespMsg(StringList failedEntryIDList) :
         BaseType(NETMSGTYPE_CheckAndRepairDupInodeResp), failedEntryIDList(std::move(failedEntryIDList))
      {
      }

      CheckAndRepairDupInodeRespMsg() : BaseType(NETMSGTYPE_CheckAndRepairDupInodeResp)
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

#endif /* CHECKANDREPAIRDUPINODERESPMSG_H */
