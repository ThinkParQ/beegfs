#include <common/net/sock/RDMASocket.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/toolkit/ListTk.h>

#include <linux/if_arp.h>
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <net/sock.h>


#define NIC_STRING_LEN  1024


static bool __NIC_fillNicAddress(struct net_device* dev, NicAddrType_t nicType,
   NicAddress* outAddr);


void NIC_findAll(StrCpyList* allowedInterfaces, bool useRDMA, bool onlyRDMA,
   NicAddressList* outList)
{
   // find standard TCP/IP interfaces
   __NIC_findAllTCP(allowedInterfaces, outList);

   // find RDMA interfaces (based on TCP/IP interfaces query results)
   if(useRDMA && RDMASocket_rdmaDevicesExist() )
   {
      NicAddressList tcpInterfaces;

      NicAddressList_init(&tcpInterfaces);

      __NIC_findAllTCP(allowedInterfaces, &tcpInterfaces);

      __NIC_filterInterfacesForRDMA(&tcpInterfaces, outList);

      ListTk_kfreeNicAddressListElems(&tcpInterfaces);
      NicAddressList_uninit(&tcpInterfaces);
   }

   if (onlyRDMA)
   {
      NicAddressListIter nicIter;
      NicAddressListIter_init(&nicIter, outList);
      while (!NicAddressListIter_end(&nicIter))
      {
         NicAddress* nicAddr = NicAddressListIter_value(&nicIter);
         if (nicAddr->nicType != NICADDRTYPE_RDMA)
         {
            nicIter = NicAddressListIter_remove(&nicIter);
            kfree(nicAddr);
         }
         else
            NicAddressListIter_next(&nicIter);
      }
   }
}

void __NIC_findAllTCP(StrCpyList* allowedInterfaces, NicAddressList* outList)
{
   struct net_device *dev;

   // find standard TCP/IP interfaces

   // foreach network device
   for (dev = first_net_device(&init_net); dev; dev = next_net_device(dev))
   {
      NicAddress* nicAddr = (NicAddress*)os_kmalloc(sizeof(NicAddress) );
      ssize_t metricByListPos = 0;

      if(!nicAddr)
      {
         printk_fhgfs(KERN_WARNING, "%s:%d: memory allocation failed. size: %zu\n",
            __func__, __LINE__, sizeof(*nicAddr) );
         return;
      }

      if(__NIC_fillNicAddress(dev, NICADDRTYPE_STANDARD, nicAddr) &&
         (!StrCpyList_length(allowedInterfaces) ||
          ListTk_listContains(nicAddr->name, allowedInterfaces, &metricByListPos) ) )
      {
         NicAddressList_append(outList, nicAddr);
      }
      else
      { // netdevice rejected => clean up
         kfree(nicAddr);
      }
   }
}

bool __NIC_fillNicAddress(struct net_device* dev, NicAddrType_t nicType, NicAddress* outAddr)
{
   struct ifreq ifr;
   struct in_device* in_dev;
   struct in_ifaddr *ifa;

#ifdef BEEGFS_RDMA
   outAddr->ibdev = NULL;
#endif
   // name
   strcpy(outAddr->name, dev->name);


   // SIOCGIFFLAGS:
   // get interface flags
   ifr.ifr_flags = dev_get_flags(dev);

   if(ifr.ifr_flags & IFF_LOOPBACK)
      return false; // loopback interface => skip

   ifr.ifr_hwaddr.sa_family = dev->type;

   // select which hardware types to process
   // (on Linux see /usr/include/linux/if_arp.h for the whole list)
   switch(ifr.ifr_hwaddr.sa_family)
   {
      case ARPHRD_LOOPBACK:
         return false;

      default:
      {
         // make sure we allow SDP for IB only (because an SDP socket domain is valid for other
         // NIC types as well, but cannot connect between different NIC types
         if( (nicType == NICADDRTYPE_SDP) && (ifr.ifr_hwaddr.sa_family != ARPHRD_INFINIBAND) )
            return false;
      } break;
   }


   // copy nicType
   outAddr->nicType = nicType;

   // ip address
   // note: based on inet_gifconf in /net/ipv4/devinet.c

   in_dev = __in_dev_get_rtnl(dev);
   if(!in_dev)
   {
      printk_fhgfs_debug(KERN_NOTICE, "found interface without in_dev: %s\n", dev->name);
      return false;
   }

   ifa = in_dev->ifa_list;
   if(!ifa)
   {
      printk_fhgfs_debug(KERN_NOTICE, "found interface without ifa_list: %s\n", dev->name);
      return false;
   }

   outAddr->ipAddr.s_addr = ifa->ifa_local; // ip address

   // code to read multiple addresses
   /*
   for (; ifa; ifa = ifa->ifa_next)
   {
      (*(struct sockaddr_in *)&ifr.ifr_addr).sin_family = AF_INET;
      (*(struct sockaddr_in *)&ifr.ifr_addr).sin_addr.s_addr =
                        ifa->ifa_local;
   }
   */

   return true;
}

/**
 * @return static string (not alloc'ed, so don't free it).
 */
const char* NIC_nicTypeToString(NicAddrType_t nicType)
{
   switch(nicType)
   {
      case NICADDRTYPE_RDMA: return "RDMA";
      case NICADDRTYPE_STANDARD: return "TCP";
      case NICADDRTYPE_SDP: return "SDP";

      default: return "<unknown>";
   }
}

/**
 * @return string will be kalloced and must be kfreed later
 */
char* NIC_nicAddrToString(NicAddress* nicAddr)
{
   char* nicAddrStr;
   char ipStr[NICADDRESS_IP_STR_LEN];
   const char* typeStr;

   nicAddrStr = (char*)os_kmalloc(NIC_STRING_LEN);

   NicAddress_ipToStr(nicAddr->ipAddr, ipStr);

   if(nicAddr->nicType == NICADDRTYPE_RDMA)
      typeStr = "RDMA";
   else
   if(nicAddr->nicType == NICADDRTYPE_SDP)
      typeStr = "SDP";
   else
   if(nicAddr->nicType == NICADDRTYPE_STANDARD)
      typeStr = "TCP";
   else
      typeStr = "Unknown";

   snprintf(nicAddrStr, NIC_STRING_LEN, "%s[ip addr: %s; type: %s]", nicAddr->name, ipStr, typeStr);

   return nicAddrStr;
}

bool NIC_supportsSDP(NicAddressList* nicList)
{
   bool sdpSupported = false;

   NicAddressListIter iter;
   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      if(NicAddressListIter_value(&iter)->nicType == NICADDRTYPE_SDP)
      {
         sdpSupported = true;
         break;
      }
   }

   return sdpSupported;
}

bool NIC_supportsRDMA(NicAddressList* nicList)
{
   bool rdmaSupported = false;

   NicAddressListIter iter;
   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      if(NicAddressListIter_value(&iter)->nicType == NICADDRTYPE_RDMA)
      {
         rdmaSupported = true;
         break;
      }
   }

   return rdmaSupported;
}

void NIC_supportedCapabilities(NicAddressList* nicList, NicListCapabilities* outCapabilities)
{
   outCapabilities->supportsSDP = NIC_supportsSDP(nicList);
   outCapabilities->supportsRDMA = NIC_supportsRDMA(nicList);
}


bool __NIC_checkSDPAvailable(void)
{
   Socket* sock = (Socket*)StandardSocket_construct(PF_SDP, SOCK_STREAM, 0);
   if(!sock)
      return false;

   Socket_virtualDestruct(sock);

   return true;
}

/**
 * Checks a list of TCP/IP interfaces for RDMA-capable interfaces.
 */
void __NIC_filterInterfacesForRDMA(NicAddressList* nicList, NicAddressList* outList)
{
   // Note: This works by binding an RDMASocket to each IP of the passed list.

   NicAddressListIter iter;
   NicAddressListIter_init(&iter, nicList);

   for( ; !NicAddressListIter_end(&iter); NicAddressListIter_next(&iter) )
   {
      RDMASocket rdmaSock;
      Socket* sock = (Socket*)&rdmaSock;
      NicAddress* nicAddr = NicAddressListIter_value(&iter);
      bool bindRes;

      if(!RDMASocket_init(&rdmaSock, nicAddr->ipAddr, NULL) )
         continue;

      bindRes = sock->ops->bindToAddr(sock, nicAddr->ipAddr, 0);

      if(bindRes)
      { // we've got an RDMA-capable interface => append it to outList
         NicAddress* nicAddrCopy = os_kmalloc(sizeof(NicAddress) );

         *nicAddrCopy = *nicAddr;

#ifdef BEEGFS_RDMA
         nicAddrCopy->ibdev = rdmaSock.ibvsock.cm_id->device;
#endif
         nicAddrCopy->nicType = NICADDRTYPE_RDMA;

         NicAddressList_append(outList, nicAddrCopy);
      }

      sock->ops->uninit(sock);
   }
}
