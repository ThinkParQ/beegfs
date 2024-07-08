#include "ModeSetState.h"

#include <common/net/message/nodes/SetTargetConsistencyStatesMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/UiTk.h>

#include <program/Program.h>

#define MODESETSTATE_ARG_TARGETID   "--targetid"
#define MODESETSTATE_ARG_NODEID     "--nodeid"
#define MODESETSTATE_ARG_STATE      "--state"
#define MODESETSTATE_ARG_STATE_GOOD "good"
#define MODESETSTATE_ARG_STATE_BAD  "bad"
#define MODESETSTATE_ARG_FORCE      "--force"

int ModeSetState::execute()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   uint16_t cfgTargetID = 0;

   TargetConsistencyState cfgState;

   ModeHelper::printEnterpriseFeatureMsg();

   if (!ModeHelper::checkRootPrivileges())
      return APPCODE_RUNTIME_ERROR;

   nodeType = ModeHelper::nodeTypeFromCfg(cfg);
   if (this->nodeType != NODETYPE_Meta && this->nodeType != NODETYPE_Storage)
   {
      std::cerr << "Invalid or missing node type." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   StringMapIter iter = cfg->find(MODESETSTATE_ARG_TARGETID);
   if(iter != cfg->end() )
   {
      if (nodeType == NODETYPE_Meta)
      {
         std::cerr << "TargetIDs are only supported when setting state of storage targets. "
            "For metadata servers, plase use the --nodeID parameter." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      bool isNumericRes = StringTk::isNumeric(iter->second);
      if(!isNumericRes)
      {
         std::cerr << "Invalid targetID given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgTargetID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODESETSTATE_ARG_NODEID);
   if (iter != cfg->end())
   {
      if (nodeType == NODETYPE_Storage)
      {
         std::cerr << "NodeIDs are only supported when setting state of metadata nodes. "
            "For storage targets, please use the --targetID parameter." << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      bool isNumericRes = StringTk::isNumeric(iter->second);
      if (!isNumericRes)
      {
         std::cerr << "Invalid nodeID given (must be numeric): " << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }

      cfgTargetID = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODESETSTATE_ARG_STATE);
   if (iter != cfg->end())
   {
      if (iter->second == MODESETSTATE_ARG_STATE_GOOD)
      {
         cfg->erase(iter); // erase the "--state"

         iter = cfg->find(MODESETSTATE_ARG_FORCE);
         if (iter == cfg->end())
         {
            std::cerr << "If state should be set to \"good\", --force must be given." << std::endl;
            return APPCODE_INVALID_CONFIG;
         }

         cfg->erase(iter); // erase the "--force"
         cfgState = TargetConsistencyState_GOOD;
      }
      else if (iter->second == MODESETSTATE_ARG_STATE_BAD)
      {
         cfgState = TargetConsistencyState_BAD;
         cfg->erase(iter); // erase the "--state"
      }
      else
      {
         std::cerr << "Invalid state given (must be \"good\" or \"bad\"): "
            << iter->second << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }
   else
   {
      std::cerr << "State must be specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   if (ModeHelper::checkInvalidArgs(cfg))
      return APPCODE_INVALID_CONFIG;

   if (!uitk::userYNQuestion("WARNING!\n\nThis command is very dangerous and can cause serious data "
         "loss.\n"
         "It should not be used under normal circumstances. It is absolutely recommended to contact "
         "support before proceeding."))
      return APPCODE_INVALID_CONFIG;

   return doSet(cfgTargetID, cfgState);
}

void ModeSetState::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --nodetype=<nodetype>           The node type (metadata, storage)." << std::endl;
   std::cout << "  --targetid=<targetID>           The ID of the target whose state should be" << std::endl;
   std::cout << "                                  set." << std::endl;
   std::cout << "                                  (only for nodetype=storage)" << std::endl;
   std::cout << "  --nodeid=<nodeID>               The ID of the node whose state should be set." << std::endl;
   std::cout << "                                  (only for nodetype=metadata)" << std::endl;
   std::cout << "  --state=<state>                 The state to be set (\"good\" or \"bad\")." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode can be used to forcefully set the state of a target or node. This is" << std::endl;
   std::cout << " useful to manually set a target or node to the state \"bad\", or to resolve a" << std::endl;
   std::cout << " situation in which both buddies in a buddy mirror group are in the state" << std::endl;
   std::cout << " \"needs-resync\"." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set a metadata node to \"bad\"" << std::endl;
   std::cout << "  $ beegfs-ctl --setstate --nodetype=metadata --nodeid=10 --state=bad" << std::endl;
}

int ModeSetState::doSet(uint16_t targetID, TargetConsistencyState state)
{
   App* app = Program::getApp();

   // Send message to mgmtd
   auto nodes = app->getMgmtNodes();
   NodeHandle node = nodes->referenceFirstNode();

   UInt16List targetIDs(1, targetID);
   UInt8List states(1, (uint8_t)state);

   SetTargetConsistencyStatesMsg msg(nodeType, &targetIDs, &states, false);

   {
      const auto respMsgRaw = MessagingTk::requestResponse(*node, msg,
            NETMSGTYPE_SetTargetConsistencyStatesResp);

      if (!respMsgRaw)
      {
         std::cerr << "Communication with node not successful." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }

      SetTargetConsistencyStatesRespMsg* respMsgCast =
         reinterpret_cast<SetTargetConsistencyStatesRespMsg*>(respMsgRaw.get());

      if (respMsgCast->getResult() != FhgfsOpsErr_SUCCESS)
      {
         std::cerr << "Management host did not accept state change. Error: "
            << respMsgCast->getResult() << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }

   // Send message to node
   if (nodeType == NODETYPE_Storage)
   {
      TargetMapper* targetMapper = app->getTargetMapper();
      FhgfsOpsErr err;

      nodes = app->getStorageNodes();
      node = nodes->referenceNodeByTargetID(targetID, targetMapper, &err);

      if (!node)
      {
         std::cerr << "Unable to resolve node for target ID " << targetID <<
            ". Error: " << err << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }
   else
   {
      nodes = app->getMetaNodes();
      node = nodes->referenceNode(NumNodeID(targetID));

      if (!node)
      {
         std::cerr << "Unable to resolve node for node ID " << targetID << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }

   {
      const auto respMsgRaw = MessagingTk::requestResponse(*node, msg,
            NETMSGTYPE_SetTargetConsistencyStatesResp);

      if (!respMsgRaw)
      {
         std::cerr << "Communication with node not successful." << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }

      auto respMsgCast = reinterpret_cast<SetTargetConsistencyStatesRespMsg*>(respMsgRaw.get());

      if (respMsgCast->getResult() != FhgfsOpsErr_SUCCESS)
      {
         std::cerr << "Node did not accept state change. Error: "
            << respMsgCast->getResult() << std::endl;
         return APPCODE_RUNTIME_ERROR;
      }
   }

   std::cout << "Successfully set state." << std::endl;

   return APPCODE_NO_ERROR;
}
