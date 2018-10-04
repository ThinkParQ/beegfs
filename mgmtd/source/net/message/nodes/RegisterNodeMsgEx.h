#ifndef REGISTERNODEMSGEX_H_
#define REGISTERNODEMSGEX_H_

#include <common/net/message/nodes/RegisterNodeMsg.h>
#include <common/nodes/NodeStoreServers.h>

class RegisterNodeMsgEx : public RegisterNodeMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

      static std::shared_ptr<Node> constructNode(NodeType nodeType, std::string nodeID,
         NumNodeID nodeNumID, unsigned short portUDP, unsigned short portTCP,
         NicAddressList& nicList);
      static bool checkNewServerAllowed(AbstractNodeStore* nodeStore, NumNodeID nodeNumID,
         NodeType nodeType);
      static void processIncomingRoot(NumNodeID rootNumID, NodeType nodeType,
         bool rootIsBuddyMirrored);
      static void processNewNode(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType,
         NicAddressList* nicList, std::string sourcePeer);
};

#endif /* REGISTERNODEMSGEX_H_ */
