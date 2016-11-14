/*
 * Count filesystem operations, methods of class NodeOpStats
 */
#include "NodeOpStats.h"

/**
 * @param cookieIP  - If several transfers are required to transfer the map to the client,
 *                    cookieIP is the last IP (in the vector) of the last transfer. We will then
 *                    continue to tranfer the map for IP *after* cookieIP
 * @param bufLen    - maximum size to be used for serialization and so for outVec
 * @param vec       - The map is encoded as vector. Format is:
 *                    numOPs, IP1, opCounter1, opCounter2, ..., opCounterN,
 *                    IP2, opCounter1, opCounter2, ..., opCounterN, IP3, ...
 */
bool NodeOpStats::mapToUInt64Vec(uint64_t cookieIP, size_t bufLen, UInt64Vector *outVec)
{
   SafeRWLock safeLock(&lock, SafeRWLock_READ);

   ClientNodeOpCounterMapIter mapIter;
   if (cookieIP == ~0UL)
      // NOTE: This is a bit tricky. For some reasons we also have to deal with cookieIP = 0
      // and as it is an unsigned, we also cannot initialize with '-1'. Therefore fhgfs-ctl
      // sends ~0 to notifify us that no cookie is set.
      mapIter = nodeCounterMap.begin();
   else
      mapIter = nodeCounterMap.upper_bound(cookieIP);

   if (mapIter == nodeCounterMap.end())
   {
      safeLock.unlock();
      return true;
   }

   // max number of IPs and their counters that fit into the vector
   unsigned maxNumIPs = getMaxIPsPerVector(&mapIter->second, bufLen);

   // make the vector sufficiently large
   reserveVector(bufLen, outVec);

   // pre-allocate the meta-elements in the vector with zeros
   for (int i=0; i<NODE_OPS_POS_FIRSTDATAELEMENT; i++)
      outVec->push_back(0);

   // below we access the fields in the vector directly with vector->at(enum-POS) to allow to simply
   // update the enum, without the need to rewrite all the code or to require to work in dedicated
   // (error-prone) order

   // VERY FIRST ELEMENT IN THE VECTOR ARE THE NUMBER OF OPs
   outVec->at(NODEOPS_POS_NUMOPS) = getNumOpCounters(&mapIter->second);

   uint64_t moreData = 0;
   outVec->at(NODE_OPS_POS_MORE_DATA) = moreData; // "0" if all stats fit into the vector

   outVec->at(NODE_OPS_POS_LAYOUT_VERSION) = OPCOUNTER_VEC_LAYOUT_VERS;

   // iterate over all IPs in the map and add IP and op-counters to the vector
   unsigned numIPs = 0; // number of IPs we have stored in the vector
   while (mapIter != nodeCounterMap.end() && numIPs < maxNumIPs)
   {
      // NOTE: we use push_back here, so no absolute enum based positions. Therefore
      //       the layout of this vecotor component should not change!

      // add IP itself
      outVec->push_back(mapIter->first) ;

      // add counters belonging to that IP. Will also use push!
      mapIter->second.addCountersToVec(outVec);

      mapIter++;
      numIPs++;
   }

   if (mapIter != nodeCounterMap.end())
      outVec->at(NODE_OPS_POS_MORE_DATA) = 1;

   safeLock.unlock();

   return true;
}

/**
 * We define a maximum transfer size for OpCounter requests of fhgfs-ctl. Several requests
 * might need to be issued from fhgfs-ctl to get all OpCounters for all clients.
 * This function returns the maximum number of IPs (including their OpCounter values)
 * fitting into a single request.
 */
int NodeOpStats::getMaxIPsPerVector(OpCounter *opCounter, size_t bufLen)
{
   // maximum number of uint64_t elements fitting into the vector
   int maxInt64VecSize = this->maxVectorSize(bufLen) / sizeof(uint64_t);

   // number of opcounters of the given opCounter object for a single client-IP
   int numOpCounters = opCounter->getNumCounter();

   // vector layout is: IP1, counter1, counter2, counterN, IP2, ...; so: numOpCounters + 1
   int maxNumIPs = ( maxInt64VecSize - NODE_OPS_POS_FIRSTDATAELEMENT) /
      (numOpCounters + OPCOUNTERVECTOR_POS_FIRSTCOUNTER);

   return maxNumIPs;
}

/**
 * Reserve the vector with the appropriate size
 */
bool NodeOpStats::reserveVector(size_t bufLen, UInt64Vector *vec)
{
   // maximum number of uint64_t elements fitting into the vector
   int maxInt64VecSize = this->maxVectorSize(bufLen) / sizeof(uint64_t);

   // resize the vector the maximum size
   vec->reserve(maxInt64VecSize);

   return true;
}
