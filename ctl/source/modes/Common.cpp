#include "Common.h"

#include <app/App.h>
#include <program/Program.h>
#include <nu/error_or.hpp>
#include <iostream>

namespace {
struct InfoDownloadError : std::error_category
{
    [[nodiscard]]
   const char* name() const noexcept override
   {
      return "Info Download Error";
   }

    [[nodiscard]]
   std::string message(int condition) const override
   {
      using ctl::common::InfoDownloadErrorCode;

      switch (static_cast<InfoDownloadErrorCode>(condition))
      {
         case InfoDownloadErrorCode::MgmtCommFailed:
            return "Management node communication failed";
         case InfoDownloadErrorCode::InfoDownloadFailed:
            return "Generic Download Error";
         case InfoDownloadErrorCode::ConfigurationError:
            return "Invalid Configuration";
      }
      return "Unknown Error";
   }
};
};

static const InfoDownloadError infoDownloadError;

const std::error_category&ctl::common::infoDownload_category()
{
   return infoDownloadError;
}

nu::error_or<std::pair<NumNodeID, std::vector<std::shared_ptr<Node>>>>
ctl::common::downloadNodes(const NodeType nodeType)
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();
   const unsigned mgmtNameResolutionRetries = 3;

   const int mgmtTimeoutMS = 2500;
   // Download the nodes into a list, not into the nodeStore yet*
   // The ones that actually reply to our heartbeat request will be sorted into the node store by
   // checkReachability.
   if (!NodesTk::waitForMgmtHeartbeat(
          nullptr, dgramLis, mgmtNodes, mgmtHost, mgmtPortUDP, mgmtTimeoutMS,
          mgmtNameResolutionRetries))
   {
      std::cerr << "Management node communication failed: " << mgmtHost << std::endl;
      return make_error_code(InfoDownloadErrorCode::MgmtCommFailed);
   }

   auto mgmtNode = mgmtNodes->referenceFirstNode();
   NumNodeID rootNodeID;

   std::vector<std::shared_ptr<Node>> nodes;
   if (!NodesTk::downloadNodes(*mgmtNode, nodeType, nodes, false, &rootNodeID))
   {
      std::cerr << "Node download failed." << std::endl;
      return make_error_code(InfoDownloadErrorCode::InfoDownloadFailed);
   }

   return std::make_pair(rootNodeID, std::move(nodes));
}


nu::error_or<std::tuple<UInt16List,UInt16List,UInt16List>> ctl::common::downloadMirrorBuddyGroups(const NodeType nodeType)
{
   const unsigned mgmtTimeoutMS = 2500;
   const unsigned mgmtNameResolutionRetries = 3;

   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();
   NodeStoreServers* mgmtNodes = app->getMgmtNodes();
   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   unsigned short mgmtPortUDP = app->getConfig()->getConnMgmtdPortUDP();

   UInt16List buddyGroupIDs;
   UInt16List primaryTargetIDs;
   UInt16List secondaryTargetIDs;


   if (nodeType != NODETYPE_Meta && nodeType != NODETYPE_Storage)
   {
      std::cerr << "Invalid node type." << std::endl;
      return make_error_code(InfoDownloadErrorCode::ConfigurationError);
   }

   // check mgmt node

   if(!NodesTk::waitForMgmtHeartbeat(
            nullptr, dgramLis, mgmtNodes, mgmtHost, mgmtPortUDP, mgmtTimeoutMS,
            mgmtNameResolutionRetries))
   {
      std::cerr << "Management node communication failed: " << mgmtHost << std::endl;
      return std::error_code(InfoDownloadErrorCode::MgmtCommFailed);
   }

   // download buddy groups

   auto mgmtNode = mgmtNodes->referenceFirstNode();

   if(!NodesTk::downloadMirrorBuddyGroups(*mgmtNode, nodeType, &buddyGroupIDs,
      &primaryTargetIDs, &secondaryTargetIDs, false) )
   {
      std::cerr << "Download of mirror buddy groups failed." << std::endl;
      return std::error_code(InfoDownloadErrorCode::InfoDownloadFailed);
   }

   return std::make_tuple(std::move(buddyGroupIDs),
                          std::move(primaryTargetIDs),
                          std::move(secondaryTargetIDs));
}
