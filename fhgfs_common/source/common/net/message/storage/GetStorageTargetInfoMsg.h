#ifndef GETSTORAGETARGETINFOMSG_H
#define GETSTORAGETARGETINFOMSG_H

#include <common/net/message/NetMessage.h>

class GetStorageTargetInfoMsg: public NetMessageSerdes<GetStorageTargetInfoMsg>
{
   public:
      /*
       * @param targetIDs list of targetIDs to get info on; maybe empty, in which case server
       * should deliver info on all targets he has attached
       */
      GetStorageTargetInfoMsg(UInt16List* targetIDs) : BaseType(NETMSGTYPE_GetStorageTargetInfo)
      {
         this->targetIDs = targetIDs;
      }

      GetStorageTargetInfoMsg() : BaseType(NETMSGTYPE_GetStorageTargetInfo)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->targetIDs, obj->parsed.targetIDs);
      }

   private:
      const UInt16List* targetIDs; // not owned by this object

      // for deserializaztion
      struct {
         UInt16List targetIDs;
      } parsed;

   public:
      const UInt16List& getTargetIDs()
      {
         return *targetIDs;
      }
};

#endif /*GETSTORAGETARGETINFOMSG_H*/
