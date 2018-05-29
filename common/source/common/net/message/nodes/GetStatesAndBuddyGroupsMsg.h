#ifndef GETSTATESANDBUDDYGROUPSMSG_H_
#define GETSTATESANDBUDDYGROUPSMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/Common.h>

class GetStatesAndBuddyGroupsMsg : public SimpleIntMsg
{
   public:
      /**
       * @param nodeType type of states to download (meta, storage).
       */
      GetStatesAndBuddyGroupsMsg(NodeType nodeType) :
         SimpleIntMsg(NETMSGTYPE_GetStatesAndBuddyGroups, nodeType)
      {
      }

      /**
       * For deserialization only
       */
      GetStatesAndBuddyGroupsMsg() : SimpleIntMsg(NETMSGTYPE_GetTargetStates)
      {
      }

   private:


   public:
      // getters & setters
      NodeType getNodeType()
      {
         return (NodeType)getValue();
      }
};

#endif /* GETSTATESANDBUDDYGROUPSMSG_H_ */
