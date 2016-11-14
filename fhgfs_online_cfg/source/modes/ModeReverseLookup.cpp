#include <app/App.h>
#include <common/toolkit/NodesTk.h>
#include <program/Program.h>
#include "ModeReverseLookup.h"
#include <common/net/message/storage/lookup/FindEntrynameMsg.h>
#include <common/net/message/storage/lookup/FindEntrynameRespMsg.h>
#include <common/net/message/storage/lookup/FindLinkOwnerMsg.h>
#include <common/net/message/storage/lookup/FindLinkOwnerRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/storage/Metadata.h>


#define MODEREVERSELOOKUP_ARG_VERBOSE           "--verbose"


int ModeReverseLookup::execute()
{
   const int mgmtTimeoutMS = 2500;

   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStore* mgmtNodes = app->getMgmtNodes();
   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();

   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   // check args
   StringMapIter iter = cfg->find(MODEREVERSELOOKUP_ARG_VERBOSE);
   if(iter != cfg->end() )
   {
      cfgVerbose = true;
      cfg->erase(iter);
   }

   // get the ID from the command line
   if(cfg->empty() )
   {
      std::cerr << "No ID specified." << std::endl;
      return APPCODE_INVALID_RESULT;
   }

   std::string inputID = cfg->begin()->first;
   cfg->erase(cfg->begin() );


   // check mgmt node
   if (!NodesTk::waitForMgmtHeartbeat(NULL, dgramLis, mgmtNodes, mgmtHost, mgmtPortUDP,
      mgmtTimeoutMS))
   {
      std::cerr << "Management node communication failed: " << mgmtHost << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   NodeType nodeType = NODETYPE_Meta;
   std::vector<NodeHandle> metaNodes;

   std::string id = inputID;
   std::string pathName = "";

   if (!NodesTk::downloadNodes(*mgmtNode, nodeType, metaNodes, false))
   {
      std::cerr << "Node download failed." << std::endl;
      retVal = APPCODE_RUNTIME_ERROR;
      goto cleanup_mgmt;
   }

   retVal = APPCODE_NO_ERROR;

   // construct the pathName
   while ( id.compare(META_ROOTDIR_ID_STR) != 0)
   {
      std::string name="";
      std::string parentID="";
      if (lookupId(id, metaNodes, &name, &parentID))
      {
         if (name.empty())
         {
            // if the name is still empty we could not find a part of the path name => error
            retVal = APPCODE_INVALID_RESULT;
            std::cerr << "No records could be found for ID " << inputID << "." << std::endl;
            break;
         }
         else
         {
            pathName = "/" + name + pathName;
            id = parentID;
            if (cfgVerbose)
            {
               // if verbose print part of the path name immediately
               std::cout << "Current Path: " << pathName << std::endl;
            }
         }
      }
      else
      {
         retVal = APPCODE_RUNTIME_ERROR;
         std::cerr << "Internal error!" << std::endl;
         break;
      }
   }

cleanup_mgmt:
   if (retVal == APPCODE_NO_ERROR)
   {
      std::cout << "Path: " << pathName << std::endl;
   }

   return retVal;
}

bool ModeReverseLookup::lookupId(std::string id, const std::vector<NodeHandle>& metaNodes,
   std::string *outName, std::string *outParentID)
{
   *outName = "";
   *outParentID= "";

   // first of all try to find out where the link should be and try that MDS first
   NumNodeID linkOwnerNodeID;
   std::string linkParentPath = "";
   if (!getLinkOwnerInfo(id, metaNodes, linkOwnerNodeID, &linkParentPath))
   {
      // we did not find a file or directory with the ID anywhere, so we can abort now!
      // We return true because we do not want to throw an error, but instead just tell the
      // user that we could not find a record for his ID
      return true;
   }

   // set the parent ID, which we give back to calling function, to the suggested parent ID
   // returned by the entry
   *outParentID = linkParentPath;

   auto nodeIter = metaNodes.begin();
   Node *node = NULL;
   while (nodeIter != metaNodes.end())
   {
      Node& tmpNode = **nodeIter;
      if (tmpNode.getNumID() == linkOwnerNodeID)
      {
         node = &tmpNode;
         break;
      }
      else
      {
         nodeIter++;
      }
   }

   // if the node is not NULL check it
   // otherwise we do not abort, although we could not find the node in the list,
   // because  maybe the node that is saved as link owner in the entry was just removed
   if (node != NULL)
   {
      if (!lookupIdOnSingleMDSFixedParent(id, node, linkParentPath, outName))
    {
      // if an error occurs we abort
       return false;
    }
   }

   // if the link was not found on the MDS that is saved in the entry, try the other MDSs,
   // (only enters loop if outName is still empty)
   nodeIter = metaNodes.begin();
   while ((nodeIter != metaNodes.end()) && (outName->empty()))
   {
      Node& node = **nodeIter;

      if (!lookupIdOnSingleMDS(id, &node, outName, outParentID))
      {
         // if an error occurs we abort
         return false;
      }
      nodeIter++;
   }
   return true;
}

bool ModeReverseLookup::lookupIdOnSingleMDS(std::string id, Node *node, std::string *outName,
   std::string *outParentID)
{
   if (node != NULL)
   {
      bool entriesLeft = true;
      long currentDirLoc = 0;
      long lastEntryLoc = 0;
      while ((entriesLeft) && (outName->empty()))
      {
         bool commRes;
         char *respBuf = NULL;
         NetMessage *respMsg = NULL;
         FindEntrynameMsg findEntrynameMsg(id, currentDirLoc, lastEntryLoc);
         commRes = MessagingTk::requestResponse(*node, &findEntrynameMsg,
            NETMSGTYPE_FindEntrynameResp, &respBuf, &respMsg);
         if (commRes)
         {
            FindEntrynameRespMsg *findEntrynameRespMsg = (FindEntrynameRespMsg*) respMsg;
            currentDirLoc = findEntrynameRespMsg->getCurrentDirLoc();
            lastEntryLoc = findEntrynameRespMsg->getLastEntryLoc();
            entriesLeft = findEntrynameRespMsg->getEntriesLeft();
            *outName = findEntrynameRespMsg->getEntryName();
            *outParentID = findEntrynameRespMsg->getParentID();
         }
         else
         {
            std::cerr << "Communication error with node " << node->getID() << " occurred."
               << std::endl;
            SAFE_FREE(respBuf);
            SAFE_DELETE(respMsg);
            return false;
         }
         SAFE_FREE(respBuf);
         SAFE_DELETE(respMsg);
      }
   }
   return true;
}

bool ModeReverseLookup::lookupIdOnSingleMDSFixedParent(std::string id, Node *node,
   std::string parentID, std::string *outName)
{
   *outName = "";
   if (node != NULL)
   {
      bool commRes;
      char *respBuf = NULL;
      NetMessage *respMsg = NULL;
      FindEntrynameMsg findEntrynameMsg(id, parentID);
      commRes = MessagingTk::requestResponse(*node, &findEntrynameMsg, NETMSGTYPE_FindEntrynameResp,
         &respBuf, &respMsg);
      if (commRes)
      {
         FindEntrynameRespMsg *findEntrynameRespMsg = (FindEntrynameRespMsg*) respMsg;
         *outName = findEntrynameRespMsg->getEntryName();
      }
      else
      {
         std::cerr << "Communication error with node " << node->getID() << " occurred." << std::endl;
         SAFE_FREE(respBuf);
         SAFE_DELETE(respMsg);
         return false;
      }
      SAFE_FREE(respBuf);
      SAFE_DELETE(respMsg);
   }
   return true;
}

bool ModeReverseLookup::getLinkOwnerInfo(std::string id, const std::vector<NodeHandle>& metaNodes,
   NumNodeID& outLinkOwnerID, std::string *outParentID)
{
   auto nodeIter = metaNodes.begin();
   while (nodeIter != metaNodes.end())
   {
      Node& node = **nodeIter;

      bool commRes;
      char *respBuf = NULL;
      NetMessage *respMsg = NULL;
      FindLinkOwnerMsg findLinkOwnerMsg(id);
      commRes = MessagingTk::requestResponse(node, &findLinkOwnerMsg,
         NETMSGTYPE_FindLinkOwnerResp, &respBuf, &respMsg);
      if (commRes)
      {
         FindLinkOwnerRespMsg *findLinkOwnerRespMsg = (FindLinkOwnerRespMsg*) respMsg;
         if (findLinkOwnerRespMsg->getResult() == 0)
         {
            outLinkOwnerID = findLinkOwnerRespMsg->getLinkOwnerNodeID();
            *outParentID = findLinkOwnerRespMsg->getParentDirID();
            SAFE_FREE(respBuf);
            SAFE_DELETE(respMsg);
            return true;
         }
      }
      else
      {
         std::cerr << "Communication error with node " << node.getID() << " occurred." << std::endl;
         SAFE_FREE(respBuf);
         SAFE_DELETE(respMsg);
         return false;
      }
      SAFE_FREE(respBuf);
      SAFE_DELETE(respMsg);

      nodeIter++;
   }

   // OK, we did not find a file or directory with the ID anywhere,
   // so basically we can abort now in the main app!
   outLinkOwnerID = NumNodeID();
   return false;
}


void ModeReverseLookup::printHelp()
{
    std::cout << "MODE ARGUMENTS:" << std::endl;
    std::cout << " Mandatory:" << std::endl;
    std::cout << "  <entryID>               The entryID of a file or directory to lookup." << std::endl;
    std::cout << std::endl;
    std::cout << " Optional:" << std::endl;
    std::cout << "  --verbose               Print every part of the path name immediately when it" << std::endl;
    std::cout << "                          is resolved. (Path names are resolved incrementally.)" << std::endl;
    std::cout << std::endl;
    std::cout << "USAGE:" << std::endl;
    std::cout << " This mode performs a (potentially slow) reverse lookup of the path for a given" << std::endl;
    std::cout << " file or directory ID." << std::endl;
    std::cout << std::endl;
    std::cout << " Example: Lookup path for file with entryID \"ID1\"" << std::endl;
    std::cout << "  $ beegfs-ctl --reverselookup ID1" << std::endl;
}

