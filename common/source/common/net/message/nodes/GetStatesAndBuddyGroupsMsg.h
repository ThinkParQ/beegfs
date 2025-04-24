#pragma once

#include "common/nodes/NumNodeID.h"
#include <common/net/message/SimpleIntMsg.h>
#include <common/Common.h>

class GetStatesAndBuddyGroupsMsg : public NetMessageSerdes<GetStatesAndBuddyGroupsMsg>
{
   public:
      /**
       * @param nodeType type of states to download (meta, storage).
       */
      GetStatesAndBuddyGroupsMsg(NodeType nodeType) :
         BaseType(NETMSGTYPE_GetStatesAndBuddyGroups), nodeType(nodeType), requestedByClientID(NumNodeID(0))
      {
      }

      /**
       * For deserialization only
       */
      GetStatesAndBuddyGroupsMsg() : BaseType(NETMSGTYPE_GetStatesAndBuddyGroups)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeType
            % obj->requestedByClientID;
      }

   private:
      int32_t nodeType;
      NumNodeID requestedByClientID;

   public:
      // getters & setters
      NodeType getNodeType()
      {
         return static_cast<NodeType>(nodeType);
      }

      NumNodeID getRequestedByClientID()
      {
         return requestedByClientID;
      }
};

