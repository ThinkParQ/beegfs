#ifndef ROUTING_TABLE_H
#define ROUTING_TABLE_H

#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>
#include <common/net/sock/NetworkInterfaceCard.h>

DECLARE_NAMEDEXCEPTION(RoutingTableException, "RoutingTableException")


class Destnet;
typedef std::unordered_map<struct in_addr, Destnet, InAddrHash> DestnetMap;
typedef std::unordered_map<struct in_addr, struct in_addr, InAddrHash> IpSourceMap;

/**
 * Encapsulates lookup of a source address for a given destination
 * address according to the OS IP routing table rules.
 *
 * Note: all public methods must be const. This class implements the
 * immutable pattern to guarantee thread safety.
 */
class RoutingTable
{
   private:
      friend class RoutingTableFactory;
      std::shared_ptr<const DestnetMap> destnets;
      std::shared_ptr<const NetVector> noDefaultRouteNets;

      RoutingTable(std::shared_ptr<const DestnetMap> destnets,
                   std::shared_ptr<const NetVector> noDefaultRouteNets);

      bool findSource(const Destnet* dn, const NicAddressList& nicList, struct in_addr& addr) const;

   public:

      RoutingTable() {};

      /**
       * Locate the address of the local NIC that should be used to communicate with
       * an address. nicList determines the priority of the interfaces to test
       * and which interfaces are valid.
       *
       * @param addr the address to communicate with
       * @param nicList list of candidate NIC addresses, the local NIC list
       * @param src receives the NIC address to use
       * @return true if a route was found for addr and it matches an element of
       *    nicList.
       */
      BEEGFS_NODISCARD bool match(struct in_addr addr, const NicAddressList& nicList, struct in_addr& src) const;

      /**
       * Populate srcMap with the local IP address to use when communicating with IP addresses specified in
       * nicList.
       *
       * @param nicList the NicAddress instances to route to (i.e. peer IPs)
       * @param localNicList the NicAdderess instances to route from (i.e. local IPs)
       * @return true if at least one route is found to the IPs in nicList.
       */
      BEEGFS_NODISCARD bool loadIpSourceMap(const NicAddressList& nicList, const NicAddressList& localNicList,
         IpSourceMap &srcMap) const;

};

/**
 * Manages collection of routing table data from the OS and creation
 * of RoutingTable instances.
 */
class RoutingTableFactory
{

   private:
      bool                         initialized;
      std::shared_ptr<DestnetMap>  destnets;
      std::mutex                   destnetsMutex;

   public:
      RoutingTableFactory()
         : initialized(false) {}

      /**
       * Initialize this instance. Must be called before load().
       * This method is not thread safe.
       */
      void init();

      /**
       * Load in routing table data from the OS. Must be called before create().
       * This method is thread safe.
       *
       * @return true if the data has changed
       */
      bool load();

      /**
       * Create a RoutingTable instance using the currently loaded data.
       * This method is thread safe.
       *
       * @return the instance
       */
      RoutingTable create(std::shared_ptr<const NetVector> noDefaultRouteNets);

};

#endif
