#ifndef NICADDRESS_H_
#define NICADDRESS_H_

#include <common/Common.h>
#include <linux/if.h>


#define NICADDRESS_IP_STR_LEN    16


enum NicAddrType;
typedef enum NicAddrType NicAddrType_t;

struct NicAddress;
typedef struct NicAddress NicAddress;

struct NicListCapabilities;
typedef struct NicListCapabilities NicListCapabilities;


extern bool NicAddress_preferenceComp(const NicAddress* lhs, const NicAddress* rhs);

// inliners
static inline void NicAddress_ipToStr(struct in_addr ipAddr, char* outStr);
static inline bool NicAddress_equals(NicAddress* this, NicAddress* other);


enum NicAddrType        {NICADDRTYPE_STANDARD=0, NICADDRTYPE_SDP=1, NICADDRTYPE_RDMA=2};

struct NicAddress
{
   struct in_addr ipAddr;
   NicAddrType_t  nicType;
   char           name[IFNAMSIZ];
};

struct NicListCapabilities
{
   bool supportsSDP;
   bool supportsRDMA;
};




/**
 * @param outStr must be at least NICADDRESS_STR_LEN bytes long
 */
void NicAddress_ipToStr(struct in_addr ipAddr, char* outStr)
{
   u8* ipArray = (u8*)&ipAddr.s_addr;

   sprintf(outStr, "%u.%u.%u.%u", ipArray[0], ipArray[1], ipArray[2], ipArray[3]);
}

bool NicAddress_equals(NicAddress* this, NicAddress* other)
{
   return (this->ipAddr.s_addr == other->ipAddr.s_addr) &&
      (this->nicType == other->nicType) &&
      !strncmp(this->name, other->name, IFNAMSIZ);
}

#endif /*NICADDRESS_H_*/
