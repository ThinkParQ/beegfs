#include <app/App.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringMsg.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringRespMsg.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <program/Program.h>
#include "ModeSetMetadataMirroring.h"


int ModeSetMetadataMirroring::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   NodeList metaNodesList;
   StringMapIter iter;

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments
   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;

   if (metaNodes->getRootIsBuddyMirrored())
   {
      std::cerr << "Meta mirroring is already enabled" << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }


   // get root node
   auto rootNode = metaNodes->referenceRootNode(metaBuddyGroupMapper);

   if(unlikely(!rootNode))
   {
      std::cerr << "Unable to reference root metadata node" << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   // apply mirroring
   if(setMirroring(*rootNode) )
      retVal = APPCODE_NO_ERROR;

   return retVal;
}

void ModeSetMetadataMirroring::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " --mirrormd does not receive any arguments." << std::endl;
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

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   SetMetadataMirroringRespMsg* respMsgCast;

   FhgfsOpsErr setRemoteRes;

   SetMetadataMirroringMsg setMsg;

   // request/response
   commRes = MessagingTk::requestResponse(
      rootNode, &setMsg, NETMSGTYPE_SetMetadataMirroringResp, &respBuf, &respMsg);
   if(!commRes)
   {
      std::cerr << "Communication with server failed: " << rootNode.getNodeIDWithTypeStr() <<
         std::endl;
      goto err_cleanup;
   }

   respMsgCast = (SetMetadataMirroringRespMsg*)respMsg;

   setRemoteRes = respMsgCast->getResult();
   if(setRemoteRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Operation failed on server: " << rootNode.getNodeIDWithTypeStr() << "; " <<
         "Error: " << FhgfsOpsErrTk::toErrString(setRemoteRes) << std::endl;
      goto err_cleanup;
   }

   std::cout << "Operation succeeded." << std::endl;

   retVal = true;

err_cleanup:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}
