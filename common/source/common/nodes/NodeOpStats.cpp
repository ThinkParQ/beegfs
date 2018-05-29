#include "NodeOpStats.h"

/**
 * @param cookieIP  - If several transfers are required to transfer the map to the client,
 *                    cookieIP is the last IP (in the vector) of the last transfer. We will then
 *                    continue to tranfer the map for IP *after* cookieIP. (~0 if no cookie yet.)
 * @param bufLen    - maximum size to be used for serialization and so for outVec
 * @param wantPerUserStats - true to query per-user instead of per-client stats.
 * @param vec       - The map is encoded as vector. Format is:
 *                    numOPs, IP1, opCounter1, opCounter2, ..., opCounterN,
 *                    IP2, opCounter1, opCounter2, ..., opCounterN, IP3, ...
 */
bool NodeOpStats::mapToUInt64Vec(uint64_t cookieIP, size_t bufLen, bool wantPerUserStats,
   UInt64Vector *outVec)
{
   SafeRWLock safeLock(&lock, SafeRWLock_READ); // L O C K

   NodeOpCounterMap* counterMap = wantPerUserStats ? &userCounterMap : &clientCounterMap;
   NodeOpCounterMapIter mapIter;

   // position iter right after given cookieIP (if any)

   if (cookieIP == ~0ULL)
   {
      // NOTE: This is a bit tricky. For some reasons we also have to deal with cookieIP = 0
      // and as it is an unsigned, we also cannot initialize with '-1'. Therefore fhgfs-ctl
      // sends ~0 to notifify us that no cookie is set.
      mapIter = counterMap->begin();
   }
   else
      mapIter = counterMap->upper_bound(cookieIP);

   if (mapIter == counterMap->end() )
   { // reached end of map => nothing to return in outVec
      safeLock.unlock(); // U N L O C K
      return true;
   }

   // max number of IPs and their counters that fit into the vector
   unsigned maxNumIPs = getMaxIPsPerVector(&mapIter->second, bufLen);

   // make the vector buffer sufficiently large
   size_t numReserveIPs = BEEGFS_MIN(maxNumIPs, counterMap->size() );
   reserveVector(&mapIter->second, numReserveIPs, outVec);

   // pre-allocate the header meta-elements in the vector with zeros
   for (int i=0; i < NODE_OPS_POS_FIRSTDATAELEMENT; i++)
      outVec->push_back(0);

   // below we access the fields in the vector directly with vector->at(enum-POS) to allow to simply
   // update the enum, without the need to rewrite all the code or to require to work in dedicated
   // (error-prone) order

   // VERY FIRST ELEMENT IN THE VECTOR ARE THE NUMBER OF OPs
   outVec->at(NODEOPS_POS_NUMOPS) = mapIter->second.getNumCounter();

   outVec->at(NODE_OPS_POS_MORE_DATA) = 0; // quasi-boolean ("0" means all stats fit into vector)

   outVec->at(NODE_OPS_POS_LAYOUT_VERSION) = OPCOUNTER_VEC_LAYOUT_VERS;

   // iterate over all IPs in the map and add IP and op-counters to the vector
   unsigned numIPs = 0; // number of IPs we have stored in the vector
   while ( (mapIter != counterMap->end() ) && (numIPs < maxNumIPs) )
   {
      // NOTE: we use push_back here, so no absolute enum based positions. Therefore
      //       the layout of this vector component should not change!

      // add IP itself
      outVec->push_back(mapIter->first) ;

      // push_back counters belonging to that IP
      mapIter->second.addCountersToVec(outVec);

      mapIter++;
      numIPs++;
   }

   if (mapIter != counterMap->end() )
      outVec->at(NODE_OPS_POS_MORE_DATA) = 1;

   safeLock.unlock(); // L O C K

   return true;
}

/**
 * We define a maximum transfer size for OpCounter requests of fhgfs-ctl. Several requests
 * might need to be issued from fhgfs-ctl to get all OpCounters for all clients.
 * This function returns the maximum number of IPs (including their OpCounter values)
 * fitting into a single request (i.e. the given bufLen).
 */
int NodeOpStats::getMaxIPsPerVector(OpCounter *opCounter, size_t bufLen)
{
   // vector layout is: headerElem1..n, IP1, counterIP1_1..n, IP2, counterIP2_1..n, IP3, ...


   // maximum number of uint64_t elements fitting into the vector
   int numElems = bufLen / sizeof(uint64_t);

   // available for per-IP vectors without the space for header elements
   int numAvailableIPElems = numElems - STATS_VEC_RESERVED_ELEMENTS;

   // number of opcounters of the given opCounter object for a single client IP vector
   int numOpCounters = opCounter->getNumCounter();

   int maxNumIPs = numAvailableIPElems / (numOpCounters + OPCOUNTERVECTOR_POS_FIRSTCOUNTER);


   return maxNumIPs;
}

/**
 * Pre-alloc internal vector buffers for given number of IPs
 */
bool NodeOpStats::reserveVector(OpCounter *opCounter, size_t numIPs, UInt64Vector *outVec)
{
   int numOpCounters = opCounter->getNumCounter();
   int perIPElems = numOpCounters + OPCOUNTERVECTOR_POS_FIRSTCOUNTER;

   int totalNumElems = STATS_VEC_RESERVED_ELEMENTS + (perIPElems * numIPs);

   outVec->reserve(totalNumElems);

   return true;
}
