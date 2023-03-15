#ifndef NETWORKINTERFACECARD_H_
#define NETWORKINTERFACECARD_H_

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

#include <net/if.h>


enum NicAddrType {
   NICADDRTYPE_STANDARD,
   NICADDRTYPE_SDP,
   NICADDRTYPE_RDMA
};

/**
 * Note: Make sure this struct can be copied with the assignment operator.
 */
struct NicAddress
{
   struct in_addr ipAddr;
   NicAddrType    nicType;
   char           name[IFNAMSIZ];

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

   bool operator==(const NicAddress& o) const
   {
      return ipAddr.s_addr == o.ipAddr.s_addr && nicType == o.nicType
         && !strncmp(name, o.name, sizeof(name));
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

// used for debugging
static inline std::string NicAddressList_str(const NicAddressList& nicList)
{
   std::string r;
   char buf[64];
   for (NicAddressList::const_iterator it = nicList.begin(); it != nicList.end(); ++it)
   {
      snprintf(buf, sizeof(buf), "name=%s type=%d addr=%x, ", it->name, it->nicType, it->ipAddr.s_addr);
      r += std::string(buf);
   }
   return r;
}

class NetworkInterfaceCard
{
   public:
      static bool findAll(StringList* allowedInterfacesList, bool useRDMA,
         NicAddressList* outList);
      static bool findAllInterfaces(const StringList& allowedInterfacesList,
         NicAddressList& outList);
      static bool findByName(const char* interfaceName, NicAddress* outAddr);

      static const char* nicTypeToString(NicAddrType nicType);
      static std::string nicAddrToString(NicAddress* nicAddr);

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
      struct NicAddrComp {
         // the comparison implemented here is a model of Compare only if preferences contains
         // all possible names ever encountered or is unset.
         const StringList* preferences = nullptr;

         explicit NicAddrComp(const StringList* preferences = nullptr): preferences(preferences) {}

         bool operator()(const NicAddress& lhs, const NicAddress& rhs) const
         {
            // compares the preference of NICs
            // returns true if lhs is preferred compared to rhs
            if (preferences) {
               for (const auto& p : *preferences) {
                  if (p == lhs.name)
                     return true;
                  if (p == rhs.name)
                     return false;
               }
            }

            // prefer RDMA NICs
            if( (lhs.nicType == NICADDRTYPE_RDMA) && (rhs.nicType != NICADDRTYPE_RDMA) )
               return true;
            if( (rhs.nicType == NICADDRTYPE_RDMA) && (lhs.nicType != NICADDRTYPE_RDMA) )
               return false;

            // prefer higher ipAddr
            unsigned lhsHostOrderIP = ntohl(lhs.ipAddr.s_addr);
            unsigned rhsHostOrderIP = ntohl(rhs.ipAddr.s_addr);

            // this is the original IP-order version
            return lhsHostOrderIP > rhsHostOrderIP;
         }
      };
};


#endif /*NETWORKINTERFACECARD_H_*/
