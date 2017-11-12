#ifndef GETTARGETSTATESRESPMSG_H
#define GETTARGETSTATESRESPMSG_H

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/TargetStateStore.h>

class GetTargetStatesRespMsg : public NetMessageSerdes<GetTargetStatesRespMsg>
{
   public:
      GetTargetStatesRespMsg(UInt16List* targetIDs, UInt8List* reachabilityStates,
            UInt8List* consistencyStates) :
         BaseType(NETMSGTYPE_GetTargetStatesResp)
      {
         this->targetIDs = targetIDs;
         this->reachabilityStates = reachabilityStates;
         this->consistencyStates = consistencyStates;
      }

      GetTargetStatesRespMsg() :
         BaseType(NETMSGTYPE_GetTargetStatesResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->targetIDs, obj->parsed.targetIDs)
            % serdes::backedPtr(obj->reachabilityStates, obj->parsed.reachabilityStates)
            % serdes::backedPtr(obj->consistencyStates, obj->parsed.consistencyStates);
      }

   private:
      UInt16List* targetIDs; // not owned by this object!
      UInt8List* reachabilityStates; // not owned by this object!
      UInt8List* consistencyStates; // not owned by this object!

      // for deserialization
      struct {
         UInt16List targetIDs;
         UInt8List reachabilityStates;
         UInt8List consistencyStates;
      } parsed;

   public:
      UInt16List& getTargetIDs()
      {
         return *targetIDs;
      }

      UInt8List& getReachabilityStates()
      {
         return *reachabilityStates;
      }

      UInt8List& getConsistencyStates()
      {
         return *consistencyStates;
      }
};


#endif /* GETTARGETSTATESRESPMSG_H */
