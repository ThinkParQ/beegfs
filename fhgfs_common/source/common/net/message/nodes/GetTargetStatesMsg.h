#ifndef GETTARGETSTATESMSG_H
#define GETTARGETSTATESMSG_H

#include <common/net/message/SimpleIntMsg.h>
#include <common/Common.h>


class GetTargetStatesMsg : public SimpleIntMsg
{
   public:
      /**
       * @param nodeType type of states to download (meta, storage).
       */
      GetTargetStatesMsg(NodeType nodeType) : SimpleIntMsg(NETMSGTYPE_GetTargetStates, nodeType)
      {
      }

      /**
       * For deserialization only
       */
      GetTargetStatesMsg() : SimpleIntMsg(NETMSGTYPE_GetTargetStates)
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


#endif /* GETTARGETSTATESMSG_H */
