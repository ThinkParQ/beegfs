#ifndef NICADDRESSSTATS_H_
#define NICADDRESSSTATS_H_

#include <common/Common.h>
#include <common/toolkit/Time.h>
#ifdef BEEGFS_RDMA
#include <rdma/ib_verbs.h>
#endif

struct NicAddressStats;
typedef struct NicAddressStats NicAddressStats;

static inline void NicAddressStats_init(NicAddressStats* this, NicAddress* nic);
static inline void NicAddressStats_uninit(NicAddressStats* this);
/**
 * Called when an associated NIC has gone down. This indicates
 * that this particular statistic should not be considered for load balancing.
 */
static inline void NicAddressStats_invalidate(NicAddressStats* this);
/**
 * Called when an associated NIC has come online. This updates the internal NicAddress
 * and indicates that this particular statistic should be considered for load balancing.
 */
static inline void NicAddressStats_setValid(NicAddressStats* this, NicAddress* nic);
static inline int NicAddressStats_comparePriority(NicAddressStats* this, NicAddressStats* o,
   int numa);
static inline void NicAddressStats_updateUsed(NicAddressStats* this);
static inline void NicAddressStats_updateLastError(NicAddressStats* this);
static inline bool NicAddressStats_lastErrorExpired(NicAddressStats* this, Time* now,
   int expirationSecs);
static inline bool NicAddressStats_usable(NicAddressStats* this, int maxConns);

struct NicAddressStats
{
   NicAddress nic;
   int        established;
   int        available;
   Time       used;
   Time       lastError;
   /**
    * nicValid indicates if the NicAddress can be used for connections.
    * This may be tracking stats for a device that has gone offline.
    */
   bool       nicValid;
};

void NicAddressStats_init(NicAddressStats* this, NicAddress* nic)
{
   this->nic = *nic;
   this->established = 0;
   this->available = 0;
   this->nicValid = true;
   Time_initZero(&this->used);
   Time_initZero(&this->lastError);
}

void NicAddressStats_uninit(NicAddressStats* this)
{
}

void NicAddressStats_invalidate(NicAddressStats* this)
{
   this->nicValid = false;
#ifdef BEEGFS_RDMA
   this->nic.ibdev = NULL;
#endif
}

void NicAddressStats_setValid(NicAddressStats* this, NicAddress* nic)
{
   this->nicValid = true;
   this->nic = *nic;
}

/*
 * Compare the priority of this and o.
 *
 * Return value is < 0 if this has higher priority, > 0 if o has higher priority.
 */
int NicAddressStats_comparePriority(NicAddressStats* this, NicAddressStats* o,
   int numa)
{
   int rc;

#ifdef BEEGFS_RDMA
   // device on the same numa node as current thread has higher priority
   if (likely(this->nic.ibdev && o->nic.ibdev))
   {
      int thisNode = this->nic.ibdev->dma_device->numa_node;
      int oNode = o->nic.ibdev->dma_device->numa_node;
      if (thisNode != oNode)
      {
         if (thisNode == numa)
            return -1;
         if (oNode == numa)
            return 1;
      }
   }
#endif

   // device with more available connections has higher priority
   rc = o->available - this->available;
   if (rc != 0)
      return rc;
   // device with less established connections has higher priority
   rc = this->established - o->established;
   if (rc != 0)
      return rc;
   // device used less recently has higher priority
   return Time_compare(&this->used, &o->used);
}

void NicAddressStats_updateUsed(NicAddressStats* this)
{
   Time_setToNow(&this->used);
}

void NicAddressStats_updateLastError(NicAddressStats* this)
{
   Time_setToNow(&this->lastError);
}

bool NicAddressStats_lastErrorExpired(NicAddressStats* this, Time* now, int expirationSecs)
{
   return Time_elapsedSinceMS(now, &this->lastError) >= (expirationSecs * 1000);
}

bool NicAddressStats_usable(NicAddressStats* this, int maxConns)
{
#ifdef BEEGFS_RDMA
   if (unlikely(!this->nic.ibdev))
      return false;
#endif
   return this->available > 0 || this->established < maxConns;
}

#endif /*NICADDRESSSTATS_H_*/
