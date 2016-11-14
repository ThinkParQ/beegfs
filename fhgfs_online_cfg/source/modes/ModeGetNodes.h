#ifndef MODEGETNODES_H_
#define MODEGETNODES_H_

#include <common/Common.h>
#include "Mode.h"


class ModeGetNodes : public Mode
{
   public:
      ModeGetNodes()
      {
         cfgPrintDetails = false;
         cfgPrintNicDetails = false;
         cfgPrintFhgfsVersion = false;
         cfgCheckReachability = false;
         cfgReachabilityNumRetries = 6; // (>= 1)
         cfgReachabilityRetryTimeoutMS = 500;
         cfgPing = false;
         cfgPingRetries = 0;
         cfgConnTestNum = 0;
         cfgPrintConnRoute = false;
      }

      virtual int execute();

      static void printHelp();


   protected:

   private:
      bool cfgPrintDetails;
      bool cfgPrintNicDetails;
      bool cfgPrintFhgfsVersion;
      bool cfgCheckReachability;
      unsigned cfgReachabilityNumRetries; // (>= 1)
      unsigned cfgReachabilityRetryTimeoutMS;
      bool cfgPing;
      unsigned cfgPingRetries;
      unsigned cfgConnTestNum;
      bool cfgPrintConnRoute;

      void printNodes(const std::vector<NodeHandle>& nodes,
         const std::set<NumNodeID>& unreachableNodes, NumNodeID rootNodeID);
      void printNicList(Node& node);
      void printFhgfsVersion(Node& node);
      void printPorts(Node& node);
      void printGotRoot(Node& node, uint16_t rootNodeID);
      void printReachability(Node& node, const std::set<NumNodeID>& unreachableNodes);
      void printConnRoute(Node& node, const std::set<NumNodeID>& unreachableNodes);
};

#endif /* MODEGETNODES_H_ */
