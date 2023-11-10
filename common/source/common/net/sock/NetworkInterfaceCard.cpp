#include "NetworkInterfaceCard.h"

#include <common/toolkit/ListTk.h>
#include <common/toolkit/StringTk.h>
#include <common/app/log/LogContext.h>
#include <common/system/System.h>
#include "Socket.h"
#include "RDMASocket.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

#define IFRSIZE(numIfcReqs)      ((int)((numIfcReqs) * sizeof(struct ifreq) ) )


/**
 * find all network interfaces and check if they are capable of doing RDMA
 *
 * @return true if any usable standard interface was found
 */
bool NetworkInterfaceCard::findAll(StringList* allowedInterfacesList, bool useRDMA,
   NicAddressList* outList)
{
   bool retVal = false;

   // find standard TCP/IP interfaces
   if(findAllBySocketDomain(PF_INET, NICADDRTYPE_STANDARD, allowedInterfacesList, outList) )
      retVal = true;

   // find RDMA interfaces (based on TCP/IP interfaces query results)
   if(useRDMA && RDMASocket::rdmaDevicesExist() )
   {
      NicAddressList tcpInterfaces;

      findAllBySocketDomain(PF_INET, NICADDRTYPE_STANDARD, allowedInterfacesList, &tcpInterfaces);

      filterInterfacesForRDMA(&tcpInterfaces, outList);
   }

   return retVal;
}

/**
 * find all network interfaces. This differs from findAll because here only the interfaces are
 * (TCP and SDP) are detected. No check for there RDMA capability is performed.
 *
 * @return true if any usable standard interface was found
 */
bool NetworkInterfaceCard::findAllInterfaces(const StringList& allowedInterfacesList,
   NicAddressList& outList)
{
   bool retVal = false;

   // find standard TCP/IP interfaces
   if(findAllBySocketDomain(PF_INET, NICADDRTYPE_STANDARD, &allowedInterfacesList, &outList) )
      retVal = true;

   return retVal;
}

bool NetworkInterfaceCard::findAllBySocketDomain(int domain, NicAddrType nicType,
   const StringList* allowedInterfacesList, NicAddressList* outList)
{
   int sock = socket(domain, SOCK_STREAM, 0);
   if(sock == -1)
      return false;


   int numIfcReqs = 1; // number of interfaces that can be stored in the current buffer (can grow)

   struct ifconf ifc;
   ifc.ifc_len = IFRSIZE(numIfcReqs);
   ifc.ifc_req = NULL;

   // enlarge the buffer to store all existing interfaces
   do
   {
      numIfcReqs++; // grow bufferspace to one more interface
      SAFE_FREE(ifc.ifc_req); // free previous allocation (if any)

      // alloc buffer for interfaces query
      ifc.ifc_req = (ifreq*)malloc(IFRSIZE(numIfcReqs) );
      if(!ifc.ifc_req)
      {
         LogContext("NIC query").logErr("Out of memory");
         close(sock);

         return false;
      }

      ifc.ifc_len = IFRSIZE(numIfcReqs);

      if(ioctl(sock, SIOCGIFCONF, &ifc) )
      {
         LogContext("NIC query").logErr(
            std::string("ioctl SIOCGIFCONF failed: ") + System::getErrString() );
         free(ifc.ifc_req);
         close(sock);

         return false;
      }

      /* ifc.ifc_len was updated by ioctl, so if IRSIZE<=ifc.ifc_len then there might be more
         interfaces, so loop again with larger buffer... */

   } while (IFRSIZE(numIfcReqs) <= ifc.ifc_len);


   // foreach interface

   struct ifreq* ifr = ifc.ifc_req; // pointer to current interface

   for( ; (char*) ifr < (char*) ifc.ifc_req + ifc.ifc_len; ifr++)
   {
      NicAddress nicAddr;

      if(fillNicAddress(sock, nicType, ifr, &nicAddr) )
      {
         ssize_t metricByListPos = 0;

         if (!allowedInterfacesList->empty() &&
            !ListTk::listContains(nicAddr.name, allowedInterfacesList, &metricByListPos) )
            continue; // not in the list of allowed interfaces

         outList->push_back(nicAddr);
      }
   }


   free(ifc.ifc_req);
   close(sock);

   return true;
}

/**
 * Checks a list of TCP/IP interfaces for RDMA-capable interfaces.
 */
void NetworkInterfaceCard::filterInterfacesForRDMA(NicAddressList* list, NicAddressList* outList)
{
   // Note: This works by binding an RDMASocket to each IP of the passed list.

   if (!RDMASocket::isRDMAAvailable())
      return;

   for(NicAddressListIter iter = list->begin(); iter != list->end(); iter++)
   {
      try
      {
         auto rdmaSock = RDMASocket::create();

         rdmaSock->bindToAddr(iter->ipAddr.s_addr, 0);

         // interface is RDMA-capable => append to outList

         NicAddress nicAddr = *iter;
         nicAddr.nicType = NICADDRTYPE_RDMA;

         outList->push_back(nicAddr);
      }
      catch(SocketException& e)
      {
         // interface is not RDMA-capable in this case
      }
   }
}

/*
 * Checks for RDMA-capable interfaces in a list of TCP/IP interfaces and adds the devices as
 * new RDMA devices to the list
 *
 * @param nicList a reference to a list of TCP interfaces; RDMA interfaces will be added to the list
 *
 * @return true, if at least one RDMA-capable interface was found
 */
bool NetworkInterfaceCard::checkAndAddRdmaCapability(NicAddressList& nicList)
{
   // Note: This works by binding an RDMASocket to each IP of the passed list.
   // Note: This only checks standard interfaces (no SDP interfaces)

   NicAddressList rdmaInterfaces;

   if (RDMASocket::isRDMAAvailable())
   {
      for(NicAddressListIter iter = nicList.begin(); iter != nicList.end(); iter++)
      {
         try
         {
            if (iter->nicType == NICADDRTYPE_STANDARD)
            {
               auto rdmaSock = RDMASocket::create();

               rdmaSock->bindToAddr(iter->ipAddr.s_addr, 0);

               // interface is RDMA-capable => append to outList

               NicAddress nicAddr = *iter;
               nicAddr.nicType = NICADDRTYPE_RDMA;

               rdmaInterfaces.push_back(nicAddr);
            }
         }
         catch(SocketException& e)
         {
            // interface is not RDMA-capable in this case
         }
      }
   }

   const bool foundRdmaInterfaces = !rdmaInterfaces.empty();
   nicList.splice(nicList.end(), rdmaInterfaces);

   return foundRdmaInterfaces;
}

/**
 * This is not needed in the actual app.
 * Nevertheless, it's for some reason part of some tests.
 */
bool NetworkInterfaceCard::findByName(const char* interfaceName, NicAddress* outAddr)
{
   int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
   if(sock == -1)
      return false;

   //struct ifreq* ifr;
   struct ifreq ifrr;
   //struct sockaddr_in sa;

   //ifr = &ifrr;
   ifrr.ifr_addr.sa_family = AF_INET;
   StringTk::strncpyTerminated(ifrr.ifr_name, interfaceName, sizeof(ifrr.ifr_name) );

   int fillRes = false;
   if(!ioctl(sock, SIOCGIFADDR, &ifrr) )
      fillRes = fillNicAddress(sock, NICADDRTYPE_STANDARD, &ifrr, outAddr);

   close(sock);

   return fillRes;
}

/**
 * @param ifr interface name and IP must be set in ifr when this method is called
 */
bool NetworkInterfaceCard::fillNicAddress(int sock, NicAddrType nicType, struct ifreq* ifr,
   NicAddress* outAddr)
{
   // note: struct ifreq contains a union for everything except the name. hence, new ioctl()
   //    calls overwrite the old data.

   // IP address
   // note: must be done at the beginning because following ioctl-calls will overwrite the data
   outAddr->ipAddr = ( (struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;

   // name
   // note: must be done at the beginning because following ioctl-calls will overwrite the data
   memcpy(outAddr->name, ifr->ifr_name, sizeof(outAddr->name));

   // retrieve flags
   if(ioctl(sock, SIOCGIFFLAGS, ifr) )
      return false;

   if(ifr->ifr_flags & IFF_LOOPBACK)
      return false; // loopback interface => skip

   // skip interfaces that are not currently usable
   if(!(ifr->ifr_flags & IFF_RUNNING))
      return false;

   // hardware type and address
   if(ioctl(sock, SIOCGIFHWADDR, ifr) )
      return false;
   else
   {
      // select which hardware types to process
      // (see /usr/include/linux/if_arp.h for the whole the list)
      switch(ifr->ifr_hwaddr.sa_family)
      {
         case ARPHRD_LOOPBACK: return false;

         default:
         {
            // make sure we allow SDP for IB only (because an SDP socket domain is valid for other
            // NIC types as well, but cannot connect then, of course
            if( (nicType == NICADDRTYPE_SDP) && (ifr->ifr_hwaddr.sa_family != ARPHRD_INFINIBAND) )
               return false;
         } break;

         // explicit types no longer required, because we identify the hardware by the socket type
         // now (just the loopback should be avoided)
         /*
         case ARPHRD_INFINIBAND:
         {
            outAddr->nicType = NICADDRTYPE_SDP;
            //return false;
         } break;

         case ARPHRD_NETROM:
         case ARPHRD_ETHER:
         case ARPHRD_PPP:
         case ARPHRD_EETHER:
         case ARPHRD_IEEE802:
         {
            outAddr->nicType = NICADDRTYPE_STANDARD;
         } break;

         default:
         { // ignore everything else
            return false;
         } break;
         */
      }

      // copy nicType
      outAddr->nicType = nicType;
   }

   return true;
}

/**
 * @return static string (not alloc'ed, so don't free it).
 */
const char* NetworkInterfaceCard::nicTypeToString(NicAddrType nicType)
{
   switch(nicType)
   {
      case NICADDRTYPE_RDMA: return "RDMA";
      case NICADDRTYPE_STANDARD: return "TCP";
      case NICADDRTYPE_SDP: return "SDP";

      default: return "<unknown>";
   }
}

std::string NetworkInterfaceCard::nicAddrToString(NicAddress* nicAddr)
{
   std::string resultStr;

   resultStr += nicAddr->name;
   resultStr += "[";

   resultStr += std::string("ip addr: ") + Socket::ipaddrToStr(nicAddr->ipAddr) + "; ";
   resultStr += std::string("type: ") + nicTypeToString(nicAddr->nicType);

   resultStr += "]";

   return resultStr;
}

bool NetworkInterfaceCard::supportsSDP(NicAddressList* nicList)
{
   for(NicAddressListIter iter = nicList->begin(); iter != nicList->end(); iter++)
   {
      if(iter->nicType == NICADDRTYPE_SDP)
         return true;
   }

   return false;
}

bool NetworkInterfaceCard::supportsRDMA(NicAddressList* nicList)
{
   for(NicAddressListIter iter = nicList->begin(); iter != nicList->end(); iter++)
   {
      if(iter->nicType == NICADDRTYPE_RDMA)
         return true;
   }

   return false;
}

void NetworkInterfaceCard::supportedCapabilities(NicAddressList* nicList,
         NicListCapabilities* outCapabilities)
{
   outCapabilities->supportsSDP = supportsSDP(nicList);
   outCapabilities->supportsRDMA = supportsRDMA(nicList);
}

bool IPv4Network::parse(const std::string& cidr, IPv4Network& net)
{
   std::size_t s = cidr.find('/');
   if (s > 0 && s < cidr.length() - 1)
   {
      std::string addr = cidr.substr(0, s);
      net.prefix = std::stoi(cidr.substr(s + 1));
      if (net.prefix > 32)
         return false;

      struct in_addr ina;
      if (::inet_pton(AF_INET, addr.c_str(), &ina) == 1)
      {
         net.netmask = generateNetmask(net.prefix);
         net.address.s_addr = ina.s_addr & net.netmask.s_addr;
         return true;
      }
      else
         return false;
   }
   else
      return false;
}

