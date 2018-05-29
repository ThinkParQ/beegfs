#ifndef GETNODESMSG_H_
#define GETNODESMSG_H_

#include <common/net/message/SimpleIntMsg.h>
#include <common/nodes/Node.h>

class GetNodesMsg : public SimpleIntMsg
{
   public:
      /**
       * @param nodeType type of nodes to download (meta, storage, ...)
       */
      GetNodesMsg(NodeType nodeType) : SimpleIntMsg(NETMSGTYPE_GetNodes, nodeType)
      {
      }

      /**
       * For deserialization only
       */
      GetNodesMsg() : SimpleIntMsg(NETMSGTYPE_GetNodes)
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


#endif /*GETNODESMSG_H_*/
