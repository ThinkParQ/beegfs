#ifndef GETMIRRORBUDDYGROUPSMSG_H
#define GETMIRRORBUDDYGROUPSMSG_H

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>
#include <common/nodes/Node.h>


class GetMirrorBuddyGroupsMsg : public SimpleIntMsg
{
   public:
      /**
       * @param nodeType type of states to download (meta, storage).
       */
      GetMirrorBuddyGroupsMsg(NodeType nodeType) :
         SimpleIntMsg(NETMSGTYPE_GetMirrorBuddyGroups, nodeType)
      {
      }

      /**
       * For deserialization only
       */
      GetMirrorBuddyGroupsMsg() : SimpleIntMsg(NETMSGTYPE_GetMirrorBuddyGroups)
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


#endif /* GETMIRRORBUDDYGROUPSMSG_H */
