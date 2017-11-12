#ifndef NETWORKINTERFACECARD_H_
#define NETWORKINTERFACECARD_H_

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

#include <net/if.h>


enum NicAddrType        {NICADDRTYPE_STANDARD=0, NICADDRTYPE_SDP=1, NICADDRTYPE_RDMA=2,
                         NICADDRTYPE_NAMEDSOCK=3};

/**
 * Note: Make sure this struct can be copied with the assignment operator.
 */
struct NicAddress
{
   struct in_addr ipAddr;
   struct in_addr broadcastAddr;
   int            bandwidth; // probably not working/supported
   int            metric;
   NicAddrType    nicType;
   char           name[IFNAMSIZ];
   char           hwAddr[IFHWADDRLEN];

   static void serialize(const NicAddress* obj, Serializer& ser)
   {
      ser.putBlock(&obj->ipAddr.s_addr, sizeof(obj->ipAddr.s_addr) );
      ser.putBlock(obj->name, BEEGFS_MIN(sizeof(obj->name), SERIALIZATION_NICLISTELEM_NAME_SIZE) );
      ser % serdes::as<char>(obj->nicType);
      ser.skip(3); // PADDING
   }

   static void serialize(NicAddress* obj, Deserializer& des)
   {
      static const unsigned minNameSize =
         BEEGFS_MIN(sizeof(obj->name), SERIALIZATION_NICLISTELEM_NAME_SIZE);

      ::memset(obj, 0, sizeof(*obj) );

      des.getBlock(&obj->ipAddr.s_addr, sizeof(obj->ipAddr.s_addr) );
      des.getBlock(obj->name, minNameSize);
      obj->name[minNameSize - 1] = 0;
      des % serdes::as<char>(obj->nicType);
      des.skip(3); // PADDING
   }
};

template<>
struct ListSerializationHasLength<NicAddress> : boost::false_type {};

typedef struct NicListCapabilities
{
   bool supportsSDP;
   bool supportsRDMA;
} NicListCapabilities;


typedef std::list<NicAddress> NicAddressList;
typedef NicAddressList::iterator NicAddressListIter;


class NetworkInterfaceCard
{
   public:
      static bool findAll(StringList* allowedInterfacesList, bool useSDP, bool useRDMA,
         NicAddressList* outList);
      static bool findAllInterfaces(const StringList& allowedInterfacesList, bool useSDP,
         NicAddressList& outList);
      static bool findByName(const char* interfaceName, NicAddress* outAddr);

      static const char* nicTypeToString(NicAddrType nicType);
      static std::string nicAddrToString(NicAddress* nicAddr);
      static std::string nicAddrToStringLight(NicAddress* nicAddr);
      static std::string hwAddrToString(char* hwAddr);

      static bool supportsSDP(NicAddressList* nicList);
      static bool supportsRDMA(NicAddressList* nicList);
      static void supportedCapabilities(NicAddressList* nicList,
         NicListCapabilities* outCapabilities);

      static bool checkAndAddRdmaCapability(NicAddressList& nicList);

   private:
      NetworkInterfaceCard() {}

      static bool fillNicAddress(int sock, NicAddrType nicType, struct ifreq* ifr,
         NicAddress* outAddr);
      static bool findAllBySocketDomain(int domain, NicAddrType nicType,
         const StringList* allowedInterfacesList, NicAddressList* outList);
      static void filterInterfacesForRDMA(NicAddressList* list, NicAddressList* outList);


   public:
      // inliners

      /**
       * @return true if lhs (left-hand side) is preferred comared to rhs
       */
      static bool nicAddrPreferenceComp(const NicAddress& lhs, const NicAddress& rhs)
      {
         // compares the preference of NICs
         // returns true if lhs is preferred compared to rhs

         // prefer lower metric
         if(lhs.metric < rhs.metric)
            return true;
         if(lhs.metric > rhs.metric)
            return false;

         // prefer RDMA NICs
         if( (lhs.nicType == NICADDRTYPE_RDMA) && (rhs.nicType != NICADDRTYPE_RDMA) )
            return true;
         if( (rhs.nicType == NICADDRTYPE_RDMA) && (lhs.nicType != NICADDRTYPE_RDMA) )
            return false;

         // prefer SDP NICs
         if( (lhs.nicType == NICADDRTYPE_SDP) && (rhs.nicType == NICADDRTYPE_STANDARD) )
            return true;
         if( (rhs.nicType == NICADDRTYPE_SDP) && (lhs.nicType == NICADDRTYPE_STANDARD) )
            return false;

         // prefer higher bandwidth
         if(lhs.bandwidth > rhs.bandwidth)
            return true;
         if(lhs.bandwidth < rhs.bandwidth)
            return false;

         // prefer higher ipAddr
         unsigned lhsHostOrderIP = ntohl(lhs.ipAddr.s_addr);
         unsigned rhsHostOrderIP = ntohl(rhs.ipAddr.s_addr);

         // this is the original IP-order version
         if(lhsHostOrderIP > rhsHostOrderIP)
            return true;
         if(lhsHostOrderIP < rhsHostOrderIP)
            return false;


         /*
         // debug IP-order alternative
         if(lhsHostOrderIP < rhsHostOrderIP)
            return true;
         if(lhsHostOrderIP > rhsHostOrderIP)
            return false;
         */

         // prefer lower hwAddr
         return(memcmp(lhs.hwAddr, rhs.hwAddr, IFHWADDRLEN) < 0);
      }

};


#endif /*NETWORKINTERFACECARD_H_*/
