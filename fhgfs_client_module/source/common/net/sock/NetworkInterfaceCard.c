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
static bool __NIC_findAllBySocketDomainV2(int domain, NicAddrType_t nicType,
   StrCpyList* allowedInterfaces, NicAddressList* outList);
static bool __NIC_fillNicAddressV2(struct socket* sock, NicAddrType_t nicType,
   struct ifreq* ifr, NicAddress* outAddr);
static void __NIC_filterInterfacesForRDMAV2(NicAddressList* list, NicAddressList* outList);
static int __NIC_sockIoctlV2(struct socket* sock, int cmd, unsigned long arg);


static struct net_device* __first_netdev(void)
{
#if !defined(KERNEL_HAS_FIRST_NET_DEVICE) && !defined(KERNEL_HAS_FIRST_NET_DEVICE_NS)
   return dev_base;
#elif defined(KERNEL_HAS_FIRST_NET_DEVICE)
   return first_net_device();
#else
   return first_net_device(&init_net);
#endif
}

static struct net_device* __next_netdev(struct net_device* currentDev)
{
#if !defined(KERNEL_HAS_FIRST_NET_DEVICE) && !defined(KERNEL_HAS_FIRST_NET_DEVICE_NS)
   return currentDev->next;
#else
   return next_net_device(currentDev);
#endif
}

void NIC_findAll(StrCpyList* allowedInterfaces, bool useSDP, bool useRDMA,
   NicAddressList* outList)
{
   struct net_device *dev;

   // find standard TCP/IP interfaces
   __NIC_findAllTCP(allowedInterfaces, outList);


   // find SDP interfaces
   if(useSDP && __NIC_checkSDPAvailable() )
   {
      // foreach network device
      for(dev = __first_netdev(); dev; dev = __next_netdev(dev) )
      {
         NicAddress* nicAddr = (NicAddress*)os_kmalloc(sizeof(NicAddress) );
         ssize_t metricByListPos = 0;

         if(!nicAddr)
         {
            printk_fhgfs(KERN_WARNING, "%s:%d: memory allocation failed. size: %zu\n",
               __func__, __LINE__, sizeof(*nicAddr) );
            return;
         }

         if(__NIC_fillNicAddress(dev, NICADDRTYPE_SDP, nicAddr) &&
            (!StrCpyList_length(allowedInterfaces) ||
             ListTk_listContains(nicAddr->name, allowedInterfaces, &metricByListPos) ) )
         {
            nicAddr->metric = (int)metricByListPos;
            NicAddressList_append(outList, nicAddr);
         }
         else
         { // netdevice rejected => clean up
            kfree(nicAddr);
         }
      }
   }

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
}

void __NIC_findAllTCP(StrCpyList* allowedInterfaces, NicAddressList* outList)
{
   struct net_device *dev;

   // find standard TCP/IP interfaces

   // foreach network device
   for(dev = __first_netdev(); dev; dev = __next_netdev(dev) )
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
         nicAddr->metric = (int)metricByListPos;
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

   // name
   strcpy(outAddr->name, dev->name);


   // SIOCGIFFLAGS:
   // get interface flags
   ifr.ifr_flags = dev_get_flags(dev);

   if(ifr.ifr_flags & IFF_LOOPBACK)
      return false; // loopback interface => skip

   if(!dev->dev_addr)
   { // should probably never happen
      printk_fhgfs(KERN_NOTICE, "found interface without dev_addr: %s\n", dev->name);
      return false;
   }

   // SIOCGIFHWADDR:
   // get hardware address (MAC)
   if(!dev->addr_len || !dev->dev_addr)
      memset(ifr.ifr_hwaddr.sa_data, 0, sizeof(ifr.ifr_hwaddr.sa_data) );
   else
      memcpy(ifr.ifr_hwaddr.sa_data, dev->dev_addr,
             min(sizeof(ifr.ifr_hwaddr.sa_data), (size_t) dev->addr_len) );

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

   // copy hardware address
   memcpy(&outAddr->hwAddr, &ifr.ifr_addr.sa_data, IFHWADDRLEN);


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
   outAddr->broadcastAddr.s_addr = ifa->ifa_broadcast; // broadcast address

   // code to read multiple addresses
   /*
   for (; ifa; ifa = ifa->ifa_next)
   {
      (*(struct sockaddr_in *)&ifr.ifr_addr).sin_family = AF_INET;
      (*(struct sockaddr_in *)&ifr.ifr_addr).sin_addr.s_addr =
                        ifa->ifa_local;
   }
   */


   // SIOCGIFMETRIC:
   // Get the metric of the interface (currently not supported by the kernel)
   ifr.ifr_metric = 0;
   outAddr->metric = ifr.ifr_metric;


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
   char* resultStr;
   char ipStr[NICADDRESS_IP_STR_LEN];
   char* hwStr;
   const char* typeStr;

   nicAddrStr = (char*)os_kmalloc(NIC_STRING_LEN);

   NicAddress_ipToStr(nicAddr->ipAddr, ipStr);

   hwStr = NIC_hwAddrToString(nicAddr->hwAddr);

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

   snprintf(nicAddrStr, NIC_STRING_LEN,
      "%s[ip addr: %s; hw addr: %s; metric: %d; type: %s]",
      nicAddr->name, ipStr, hwStr, nicAddr->metric, typeStr);

   kfree(hwStr);


   resultStr = StringTk_strDup(nicAddrStr); // create string with min required size
   kfree(nicAddrStr);

   return resultStr;
}

/**
 * @return string will be kalloced and must be kfreed later
 */
char* NIC_nicAddrToStringLight(NicAddress* nicAddr)
{
   char* nicAddrStr;
   char* resultStr;
   char ipStr[NICADDRESS_IP_STR_LEN];
   char broadcastStr[NICADDRESS_IP_STR_LEN];
   char* hwStr;
   const char* typeStr;

   nicAddrStr = (char*)os_kmalloc(NIC_STRING_LEN);

   NicAddress_ipToStr(nicAddr->ipAddr, ipStr);

   NicAddress_ipToStr(nicAddr->broadcastAddr, broadcastStr);

   hwStr = NIC_hwAddrToString(nicAddr->hwAddr);

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

   snprintf(nicAddrStr, NIC_STRING_LEN,
      "%s[ip addr: %s; type: %s]", nicAddr->name, ipStr, typeStr);

   kfree(hwStr);


   resultStr = StringTk_strDup(nicAddrStr); // create string with min required size
   kfree(nicAddrStr);

   return resultStr;
}

/**
 * @return string will be kalloced and must be kfreed later
 */
char* NIC_hwAddrToString(char* hwAddr)
{
   char* hwAddrStr;
   int i;

   hwAddrStr = (char*)os_kmalloc(IFHWADDRLEN*3);
   // *3 for two letters per byte plus dots plus trailing zero


   { // first loop walk (without the dot)
      sprintf(hwAddrStr, "%2.2x", (unsigned char)hwAddr[0]);
   }

   for(i=1; i < IFHWADDRLEN; i++)
   {
      sprintf(&hwAddrStr[(i*3)-1], ".%2.2x", (unsigned char)hwAddr[i]);
   }

   return(hwAddrStr);
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

      if(!RDMASocket_init(&rdmaSock) )
         continue;

      bindRes = sock->ops->bindToAddr(sock, &nicAddr->ipAddr, 0);

      if(bindRes)
      { // we've got an RDMA-capable interface => append it to outList
         NicAddress* nicAddrCopy = os_kmalloc(sizeof(NicAddress) );

         *nicAddrCopy = *nicAddr;

         nicAddrCopy->nicType = NICADDRTYPE_RDMA;

         NicAddressList_append(outList, nicAddrCopy);
      }

      sock->ops->uninit(sock);
   }
}

/**
 * Warning: Not working, because __NIC_sockIoctlV2 cannot handle SIOCGIFCONF (see method header
 * comments for reasons).
 *
 * Note: V2 here means a new detection mechanism which is based on socket ioctls (similar to our
 * userspace approach) instead of the previous mechanism which accessed the kernel's network devices
 * list directly.
 *
 * @return true if any usable standard interface was found
 */
bool NIC_findAllV2(StrCpyList* allowedInterfaces, bool useSDP, bool useRDMA,
   NicAddressList* outList)
{
   bool retVal = false;

   // find standard TCP/IP interfaces
   if(__NIC_findAllBySocketDomainV2(PF_INET, NICADDRTYPE_STANDARD, allowedInterfaces, outList) )
      retVal = true;

   // find SDP interfaces
   if(useSDP)
   {
      __NIC_findAllBySocketDomainV2(PF_SDP, NICADDRTYPE_SDP, allowedInterfaces, outList);
   }

   // find RDMA interfaces (based on TCP/IP interfaces query results)
   if(useRDMA && RDMASocket_rdmaDevicesExist() )
   {
      NicAddressList tcpInterfaces;

      NicAddressList_init(&tcpInterfaces);

      __NIC_findAllBySocketDomainV2(PF_INET, NICADDRTYPE_STANDARD, allowedInterfaces,
         &tcpInterfaces);

      __NIC_filterInterfacesForRDMAV2(&tcpInterfaces, outList);

      ListTk_kfreeNicAddressListElems(&tcpInterfaces);
      NicAddressList_uninit(&tcpInterfaces);
   }

   return retVal;
}


/**
 * Warning: Not working, because __NIC_sockIoctlV2 cannot handle SIOCGIFCONF (see method header
 * comments for reasons).
 */
bool __NIC_findAllBySocketDomainV2(int domain, NicAddrType_t nicType,
   StrCpyList* allowedInterfaces, NicAddressList* outList)
{
   struct socket* sock;
   int sockRes;

   unsigned numIfReqs = 3; // number of interfaces that can be stored in the current ifc.ifc_req
   unsigned bufLen; // note: bufLen is separate from ifc_len, because ifc_len is updated by ioctl

   struct ifconf ifc;
   struct ifreq* ifr;

   ifc.ifc_req = NULL;


   // prepare socket for SIOCGIFCONF ioctl to query all interfaces

   sockRes = sock_create(domain, SOCK_STREAM, 0, &sock);
   if(sockRes)
   {
      printk_fhgfs(KERN_NOTICE, "%s: Unable to create socket for interface query. Error: %d\n",
         __func__, sockRes);
      return false;
   }

   // incrementally enlarge the buffer to store all existing interfaces
   do
   {
      int ioctlRes;

      SAFE_KFREE(ifc.ifc_req); // free previous allocation

      numIfReqs++; // increase so that we grow with each loop
      bufLen = numIfReqs * sizeof(struct ifreq); /* note: bufLen is separate from ifc_len,
                                                          because ifc_len is updated by ioctl */
      ifc.ifc_len = bufLen;

      // alloc buffer for interfaces query
      ifc.ifc_req = os_kmalloc(bufLen);
      if(!ifc.ifc_req)
      {
         printk_fhgfs(KERN_WARNING, "%s: out of memory. alloc size: %d", __func__, bufLen);
         goto err_cleanup;
      }

      // query interfaces
      ioctlRes = __NIC_sockIoctlV2(sock, SIOCGIFCONF, (unsigned long)&ifc);
      if(ioctlRes < 0)
      {
         printk_fhgfs(KERN_WARNING, "%s: ioctl SIOCGIFCONF failed. Error: %d", __func__, ioctlRes);
         goto err_cleanup;
      }

      // ifc.ifc_len==bufLen then there might be more interfaces, so again with larger buffer...

   } while (bufLen <= ifc.ifc_len);


   // for each interface

   for(ifr = ifc.ifc_req; (char*) ifr < (char*) ifc.ifc_req + ifc.ifc_len; ifr++)
   {
      NicAddress* nicAddr = (NicAddress*)os_kmalloc(sizeof(*nicAddr) );
      ssize_t metricByListPos = 0;

      if(!nicAddr)
      { // alloc failed
         printk_fhgfs(KERN_WARNING, "%s: out of memory. alloc size: %zu",
            __func__, sizeof(*nicAddr) );
         goto err_cleanup;
      }

      if(__NIC_fillNicAddressV2(sock, nicType, ifr, nicAddr) &&
         (!StrCpyList_length(allowedInterfaces) ||
          ListTk_listContains(nicAddr->name, allowedInterfaces, &metricByListPos) ) )
      { // interface accepted
         nicAddr->metric = (int)metricByListPos;
         NicAddressList_append(outList, nicAddr);

         continue;
      }

      // netdevice rejected => clean up
      kfree(nicAddr);
   }


   // cleanup
   kfree(ifc.ifc_req);
   sock_release(sock);

   return true;


err_cleanup:
   SAFE_KFREE(ifc.ifc_req);
   sock_release(sock);

   return false;
}

/**
 * @param ifr interface name and IP must be set in ifr when this method is called
 */
bool __NIC_fillNicAddressV2(struct socket* sock, NicAddrType_t nicType, struct ifreq* ifr,
   NicAddress* outAddr)
{
   /* note: struct ifreq contains a union for everything except the name. hence, new ioctl()
            calls overwrite the old data. */


   // IP address
   // note: must be done at the beginning because following ioctl-calls will overwrite the data
   outAddr->ipAddr = ( (struct sockaddr_in *)&ifr->ifr_addr)->sin_addr;

   // name
   // note: must be done at the beginning because following ioctl-calls will overwrite the data
   strcpy(outAddr->name, ifr->ifr_name);


   // retrieve broadcast address
   if(__NIC_sockIoctlV2(sock, SIOCGIFBRDADDR, (unsigned long)ifr) )
      return false;

   outAddr->broadcastAddr = ( (struct sockaddr_in *)&ifr->ifr_broadaddr)->sin_addr;


   // retrieve flags
   if(__NIC_sockIoctlV2(sock, SIOCGIFFLAGS, (unsigned long)ifr) )
      return false;

   if(ifr->ifr_flags & IFF_LOOPBACK)
      return false; // loopback interface => skip



   // hardware type and address
   if(__NIC_sockIoctlV2(sock, SIOCGIFHWADDR, (unsigned long)ifr) )
      return false;

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
   }

   // copy nicType
   outAddr->nicType = nicType;

   // copy hardware address
   memcpy(&outAddr->hwAddr, &ifr->ifr_addr.sa_data, IFHWADDRLEN);


   // metric
   if(__NIC_sockIoctlV2(sock, SIOCGIFMETRIC, (unsigned long)ifr) )
      return false;

   outAddr->metric = ifr->ifr_metric;


   return true;
}

/**
 * Checks a list of TCP/IP interfaces for RDMA-capable interfaces.
 */
void __NIC_filterInterfacesForRDMAV2(NicAddressList* list, NicAddressList* outList)
{
   // Note: This works by binding an RDMASocket to each IP of the passed list.

   NicAddressListIter nicIter;

   NicAddressListIter_init(&nicIter, list);

   for( ; !NicAddressListIter_end(&nicIter); NicAddressListIter_next(&nicIter) )
   {
      RDMASocket* rdmaSock;
      bool bindRes;
      NicAddress* currentNicAddr;

      rdmaSock = RDMASocket_construct();
      if(!rdmaSock)
      {
         printk_fhgfs(KERN_WARNING, "%s: out of memory or could not construct. alloc size: %zu",
            __func__, sizeof(*rdmaSock) );
         return;
      }

      currentNicAddr = NicAddressListIter_value(&nicIter);

      bindRes = Socket_bindToAddr( (Socket*)rdmaSock, &currentNicAddr->ipAddr, 0);
      if(bindRes)
      { // interface is RDMA-capable => append to outList
         NicAddress* nicAddr = (NicAddress*)os_kmalloc(sizeof(*nicAddr) );

         if(!nicAddr)
         { // alloc failed
            printk_fhgfs(KERN_WARNING, "%s: out of memory. alloc size: %zu",
               __func__, sizeof(*nicAddr) );

            Socket_virtualDestruct( (Socket*)rdmaSock);
            return;
         }

         *nicAddr = *currentNicAddr;
         nicAddr->nicType = NICADDRTYPE_RDMA;

         NicAddressList_append(outList, nicAddr);
      }

      // cleanup
      Socket_virtualDestruct( (Socket*)rdmaSock);
   }
}


/**
 * Warning: Not working, because similar to userspace socket ioctl and sock_do_ioctl(), the
 * the sock->ops->ioctl would need a fall back from socket ioctl to NIC driver for unknown cmd
 * (e.g. relevant for SIOCGIFCONF) but sock->ops->ioctl just returns -ENOIOCTLCMD here and the other
 * methods like dev_ioctl, dev_ifconf or sock_do_ioctl are not exported to kernel modules.
 */
int __NIC_sockIoctlV2(struct socket* sock, int cmd, unsigned long arg)
{
   mm_segment_t oldfs;
   int ioctlRes;

   ACQUIRE_PROCESS_CONTEXT(oldfs);

   ioctlRes = sock->ops->ioctl(sock, cmd, arg);
//   if(ioctlRes == -ENOIOCTLCMD)
//   { // similar to sock_do_ioctl, fall back to NIC driver if unknown cmd (e.g. for SIOCGIFCONF)
//      struct sock* sk = sock->sk;
//      struct net* net = sock_net(sk);
//
//      ioctlRes = dev_ioctl(net, cmd, (void __user *)arg);
//   }

   RELEASE_PROCESS_CONTEXT(oldfs);

   return ioctlRes;
}

