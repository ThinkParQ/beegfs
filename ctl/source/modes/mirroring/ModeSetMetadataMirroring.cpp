#include <app/App.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringMsg.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UiTk.h>
#include <program/Program.h>
#include "ModeSetMetadataMirroring.h"
#include <modes/Common.h>

#include <iostream>


int ModeSetMetadataMirroring::execute()
{

   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   auto cfgIter = cfg->find("--force");
   if (cfgIter != cfg->end())
   {
      cfgForce = true;
      cfg->erase(cfgIter);
   }

   StringMapIter iter;

   ModeHelper::printEnterpriseFeatureMsg();

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   if (app->getMetaRoot().getIsMirrored())
   {
      std::cerr << "Meta mirroring is already enabled" << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   auto mgmt = app->getMgmtNodes()->referenceFirstNode();

   if (verifyBuddyGroupsDefined() && (cfgForce || verifyNoClientsActive())) {
      if (setMirroring(*mgmt))
         return APPCODE_NO_ERROR;
   }

   return APPCODE_RUNTIME_ERROR;
}

void ModeSetMetadataMirroring::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << "  --force    Skip safety question." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " Enables metadata mirroring for the root directory." << std::endl;
   std::cout << " If the root directory contains any files, those files will be" << std::endl;
   std::cout << " mirrored as well. Subdirectories of the root directory will" << std::endl;
   std::cout << " not be mirrored by this operation." << std::endl;
   std::cout << std::endl;
   std::cout << " Metadata mirroring cannot be enabled for any directory other " << std::endl;
   std::cout << " than the root directory after the directory has been created." << std::endl;
   std::cout << std::endl;
   std::cout << " Metadata mirroring can be enabled for files by moving them" << std::endl;
   std::cout << " from an unmirrored directory to a mirrored directory." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: enable metadata mirroring for the root directory" << std::endl;
   std::cout << "  # beegfs-ctl --mirrormd" << std::endl;
}

bool ModeSetMetadataMirroring::setMirroring(Node& rootNode)
{
   bool retVal = false;

   SetMetadataMirroringRespMsg* respMsgCast;

   FhgfsOpsErr setRemoteRes;

   SetMetadataMirroringMsg setMsg;

   const auto respMsg = MessagingTk::requestResponse(rootNode, setMsg,
         NETMSGTYPE_SetMetadataMirroringResp);
   if (!respMsg)
   {
      std::cerr << "Communication with server failed: " << rootNode.getNodeIDWithTypeStr() <<
         std::endl;
      goto err_cleanup;
   }

   respMsgCast = (SetMetadataMirroringRespMsg*)respMsg.get();

   setRemoteRes = respMsgCast->getResult();
   if(setRemoteRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Operation failed on server: " << rootNode.getNodeIDWithTypeStr() << "; " <<
         "Error: " << setRemoteRes << std::endl;
      std::cerr << "See the server's log file for details." << std::endl;
      goto err_cleanup;
   }

   std::cout << "Operation succeeded." << std::endl;
   std::cout << std::endl;
   std::cout << "NOTE:" << std::endl;
   std::cout << " To complete activating metadata mirroring, please remount any clients and" << std::endl;
   std::cout << " restart all metadata services now." << std::endl;

   retVal = true;

err_cleanup:
   return retVal;
}

bool ModeSetMetadataMirroring::verifyNoClientsActive()
{
   return ctl::common::downloadNodes(NODETYPE_Client).reduce(
      [] (const auto& data) {
            const auto& clients = data.second;
            if (!clients.empty())
               std::cerr << "Error: There are still " << clients.size() << " clients activce.\n"
                            "To activate metadata mirroring, all cients have to stopped.\n"
                            "See beegfs-ctl --listnodes --nodetype=client for more information."
                         << std::endl;
            return clients.empty();
      },
      [] (const auto&) {
            std::cerr << "Unable to download list of clients." << std::endl;
            return false; }
   );
}

bool ModeSetMetadataMirroring::verifyBuddyGroupsDefined()
{
   return ctl::common::downloadMirrorBuddyGroups(NODETYPE_Meta).reduce(
      [] (const auto& data) {
         if (std::get<0>(data).empty()) {
            std::cerr << "There are no metadata buddy mirror groups defined." << std::endl;
            return false;
         }
         return true;
      },
      [] (const auto&) {
         return false;
      }
   );
}
