#ifndef MODEGETNODES_H_
#define MODEGETNODES_H_

#include <common/Common.h>
#include "Mode.h"


class ModeGetNodes : public Mode
{
   public:
      ModeGetNodes() :
         Mode(true), // Skip init - this way, we can only put the nodes that actually replied to
                     // the heartbeat request into the node store, so we can filter out
                     // unreachable nodes later.
         cfgPrintDetails(false),
         cfgPrintNicDetails(false),
         cfgCheckReachability(false),
         cfgNotReachableAsError(false),
         cfgReachabilityNumRetries(6), // (>= 1)
         cfgReachabilityRetryTimeoutMS(500),
         cfgPing(false),
         cfgPingRetries(0),
         cfgConnTestNum(0),
         cfgPrintConnRoute(false)
      { }

      virtual int execute();

      static void printHelp();


   protected:

   private:
      bool cfgPrintDetails;
      bool cfgPrintNicDetails;
      bool cfgCheckReachability;
      bool cfgNotReachableAsError;
      unsigned cfgReachabilityNumRetries; // (>= 1)
      unsigned cfgReachabilityRetryTimeoutMS;
      bool cfgPing;
      unsigned cfgPingRetries;
      unsigned cfgConnTestNum;
      bool cfgPrintConnRoute;

      void printNodes(const std::vector<NodeHandle>& nodes,
         const std::set<NumNodeID>& unreachableNodes, NumNodeID rootNodeID);
      void printNicList(Node& node);
      void printPorts(Node& node);
      void printGotRoot(Node& node, uint16_t rootNodeID);
      void printReachability(Node& node, const std::set<NumNodeID>& unreachableNodes);
      void printConnRoute(Node& node, const std::set<NumNodeID>& unreachableNodes);
};

#endif /* MODEGETNODES_H_ */
