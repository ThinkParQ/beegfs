/*
 * Class ClientStatsDiff methods
 */

#include "ClientStatsDiff.h"
#include <common/nodes/NodeOpStats.h>

/**
 * Compare function to order the list of vectors
 */
static bool vectorCompare(UInt64Vector* vec1, UInt64Vector* vec2)
{
   uint64_t vec1Value = vec1->at(OPCOUNTERVECTOR_POS_SUM);
   uint64_t vec2Value = vec2->at(OPCOUNTERVECTOR_POS_SUM);
   uint64_t vec1IP = vec1->at(OPCOUNTERVECTOR_POS_IP);
   uint64_t vec2IP = vec2->at(OPCOUNTERVECTOR_POS_IP);

   // compare the sum element for now.
   if (vec1Value > vec2Value)
      return true;
   else
   if (vec1Value == vec2Value && vec1IP > vec2IP) // sort by IP if two values are equal
      return true;

   return false;
}

/**
 * Get the difference vector of op counters between previous and current values
 * NOTE: The result MUST be free'ed from the caller.
 */
UInt64Vector* ClientStatsDiff::newDiffVector(uint64_t IP)
{
   LogContext log = LogContext("ClientStatsDiff ( new diff vector)");

   UInt64Vector* currentVec = ClientStats::getVectorFromMap(this->currentVecMap, IP);

   if (!currentVec)
   {
      struct in_addr in_addr = { (in_addr_t)IP };
      std::string logMessage("BUG: Missing vector for IP" + Socket::ipaddrToStr(&in_addr) );

      log.logErr(logMessage);
      std::cerr << logMessage << std::endl;
      return NULL;
   }

   std::unique_ptr<UInt64Vector> diffVec(new UInt64Vector(*currentVec)); // copy of the current values first

   UInt64Vector* oldVec     = ClientStats::getVectorFromMap(this->oldVecMap, IP);
   if (!oldVec)
      return diffVec.release(); // IP just did not do any IO last time, so old values are zero


   uint64_t oldIP = oldVec->at(OPCOUNTERVECTOR_POS_IP);
   uint64_t curIP = currentVec->at(OPCOUNTERVECTOR_POS_IP);
   if (oldIP != curIP)
   {
      struct in_addr cur_addr = { (in_addr_t)curIP };
      struct in_addr old_addr = { (in_addr_t)oldIP };
      std::string logMessage("Bug: IP mismatch between old and current vector " +
         Socket::ipaddrToStr(&old_addr) + "vs. " + Socket::ipaddrToStr(&cur_addr) );

      log.logErr(logMessage);
      std::cerr << logMessage << std::endl;
      return NULL;
   }

   /* vector length might be different, for example after an update to a different FhGFS version
    * on a server. So we need to check for it and to unify vector length.
    */
   int maxVecSize = BEEGFS_MAX(currentVec->size(), oldVec->size() );
   currentVec->resize(maxVecSize, 0);
   oldVec->resize(maxVecSize, 0);

   // set iter to first counter
   UInt64VectorIter diffIter =  diffVec->begin() + OPCOUNTERVECTOR_POS_FIRSTCOUNTER;
   UInt64VectorIter oldIter  =  oldVec->begin() + OPCOUNTERVECTOR_POS_FIRSTCOUNTER;

   do
   {
      // no checks required here, as we already checked the vector sizes above
      *diffIter -= *oldIter;
      oldIter++;
      diffIter++;
   } while (oldIter != oldVec->end() );

   return diffVec.release();
}

/**
 * Sort opDiffVectorList
 */
void ClientStatsDiff::sort()
{
   this->opDiffVectorList.sort(vectorCompare);
}

/**
 * Create a summary vector of all diff-vectors
 */
void ClientStatsDiff::sumDiffVectors()
{
   UInt64VectorListIter diffVecIter = this->opDiffVectorList.begin();
   while (diffVecIter != this->opDiffVectorList.end())
   {
      UInt64Vector* diffVec = *diffVecIter;

      if (this->sumVec.empty())
      {
         this->sumVec = *diffVec; // initialization of diffVec
         diffVecIter++;
         continue;
      }

      /* Vector sizes might be different, for example if different server versions are running,
       * so we need to unify it
       */
      unsigned maxVecSize = BEEGFS_MAX(this->sumVec.size(), diffVec->size() );
      this->sumVec.resize(maxVecSize, 0);
      diffVec->resize(maxVecSize, 0);

      unsigned i = OPCOUNTERVECTOR_POS_FIRSTCOUNTER;
      for (i=OPCOUNTERVECTOR_POS_FIRSTCOUNTER; i < maxVecSize; i++)
         this->sumVec.at(i) += diffVec->at(i);


      diffVecIter++;
      continue;
   }
}


