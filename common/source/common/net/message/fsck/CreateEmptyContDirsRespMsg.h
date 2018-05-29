#ifndef CREATEEMPTYCONTDIRSRESPMSG_H
#define CREATEEMPTYCONTDIRSRESPMSG_H

#include <common/net/message/NetMessage.h>

class CreateEmptyContDirsRespMsg : public NetMessageSerdes<CreateEmptyContDirsRespMsg>
{
   public:
      CreateEmptyContDirsRespMsg(StringList* failedDirIDs)
         : BaseType(NETMSGTYPE_CreateEmptyContDirsResp)
      {
         this->failedDirIDs = failedDirIDs;
      }

      CreateEmptyContDirsRespMsg() : BaseType(NETMSGTYPE_CreateEmptyContDirsResp)
      {
      }


   private:
      StringList* failedDirIDs;

      // for deserialization
      struct {
         StringList failedDirIDs;
      } parsed;

   public:
      StringList& getFailedIDs()
      {
         return *failedDirIDs;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->failedDirIDs, obj->parsed.failedDirIDs);
      }
};

#endif /* CREATEEMPTYCONTDIRSRESPMSG_H */
