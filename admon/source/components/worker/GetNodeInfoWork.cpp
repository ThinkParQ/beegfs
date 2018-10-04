#include "GetNodeInfoWork.h"
#include <program/Program.h>
#include <common/toolkit/StringTk.h>


void GetNodeInfoWork::process(char* bufIn, unsigned bufInLen, char* bufOut,
   unsigned bufOutLen)
{
   App* app = Program::getApp();

   GeneralNodeInfo info;

   NodeStoreServers* nodes = app->getServerStoreFromType(nodeType);
   if(!nodes)
   {
      LOG(GENERAL, ERR, "Invalid node type.", nodeType);
      return;
   }

   auto node = nodes->referenceNode(nodeID);
   if(!node)
   {
      log.logErr("Unknown nodeID: " + nodeID.str() );
      return;
   }

   if (!sendMsg(node.get(), &info))
      return;

   switch(nodeType)
   {
      case NODETYPE_Mgmt:
      {
         ((MgmtNodeEx*)node.get())->setGeneralInformation(info);
      } break;

      case NODETYPE_Meta:
      {
         ((MetaNodeEx*)node.get())->setGeneralInformation(info);
      } break;

      case NODETYPE_Storage:
      {
         ((StorageNodeEx*)node.get())->setGeneralInformation(info);
      } break;

      default: break;
   }
}

bool GetNodeInfoWork::sendMsg(Node *node, GeneralNodeInfo *outInfo)
{
   GetNodeInfoMsg getNodeInfoMsg;

   const auto respMsg = MessagingTk::requestResponse(*node, getNodeInfoMsg,
         NETMSGTYPE_GetNodeInfoResp);

   if (!respMsg)
   { // comm failed
      log.logErr(std::string("Communication failed with node: ") +
         node->getNodeIDWithTypeStr() );
      return false;
   }

   // comm succeeded

   GetNodeInfoRespMsg* getNodeInfoRespMsg = (GetNodeInfoRespMsg*)respMsg.get();
   *outInfo = getNodeInfoRespMsg->getInfo();

   return true;
}
