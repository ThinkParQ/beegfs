#pragma once


#include <common/nodes/LocalNodeConnPool.h>
#include <common/nodes/NodeConnPool.h>
#include <common/nodes/Node.h>



/**
 * This class is used by nodes/services to represent themselves e.g. in a NodeStore. It uses a
 * special conn pool for internal communication (e.g. to avoid TCP if the process is sending a msg
 * to itself, like a single mds would do for a mkdir).
 */
class LocalNode : public Node
{
   public:
      /**
       * @param portTCP not used internally, but required when connection info is exported
       */
      LocalNode(NodeType nodeType, std::string alias, NumNodeID nodeNumID, unsigned short portUDP,
            unsigned short portTCP, NicAddressList& nicList):
         Node(nodeType, std::move(alias), nodeNumID, portUDP)
      {
         this->portTCP = portTCP;

         setConnPool(new LocalNodeConnPool(*this, nicList) );
      }

   private:
      unsigned short portTCP; // not used internally, but required when connection info is exported

   public:
      virtual unsigned short getPortTCP()
      {
         return portTCP;
      }
};

