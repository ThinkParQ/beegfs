#ifndef LOCALNODE_H_
#define LOCALNODE_H_

#include <common/nodes/Node.h>
#include "LocalNodeConnPool.h"

class LocalNode : public Node
{
   public:
      LocalNode(std::string nodeID, uint16_t nodeNumID, unsigned short portUDP,
         NicAddressList& nicList) : Node(nodeID, nodeNumID, portUDP)
      {
         setConnPool(new LocalNodeConnPool(nicList) );
      }
};

#endif /*LOCALNODE_H_*/
