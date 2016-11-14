#include "ClientStats.h"

#ifndef CLIENTSTATSDIFF_H_
#define CLIENTSTATSDIFF_H_


/**
 * Just a simple class to store a list of differences between opCounterVectors
 */
class ClientStatsDiff
{
   public:

      UInt64VectorList opDiffVectorList;
      ClientNodeOpVectorMap *currentVecMap;
      ClientNodeOpVectorMap *oldVecMap;
      UInt64Vector sumVec; // summary of all diff vectors

      /**
       * Create opDiffVectorList here
       */
      ClientStatsDiff(ClientNodeOpVectorMap *currentVecMap, ClientNodeOpVectorMap *oldVecMap)
      {
         this->currentVecMap = currentVecMap;
         this->oldVecMap = oldVecMap;

         ClientNodeOpVectorMapIter mapIter = currentVecMap->begin();

         // We don't have a list of IPs, but the IPs are also stored in the map, of course
         while (mapIter != currentVecMap->end())
         {
            uint64_t ip = mapIter->first;

            UInt64Vector* diffVector = this->newDiffVector(ip);
            if (!diffVector)
            {
               continue; // some kind of error, simply ignore this vector
               mapIter++;
            }

            uint64_t vecIP = diffVector->at(OPCOUNTERVECTOR_POS_IP);
            if (ip != vecIP)
            {
               LogContext log = LogContext("ClientStatsDiff");

               struct in_addr in_addr = { (in_addr_t)ip };
               struct in_addr vec_addr = { (in_addr_t)vecIP };

               std::string logMessage("BUG: Mismatch between map and vector IP: " +
                  Socket::ipaddrToStr(&in_addr) + " vs. " + Socket::ipaddrToStr(&vec_addr) );

               log.logErr(logMessage);
               std::cerr << logMessage << std::endl;

               mapIter++;
               continue;
            }

            this->opDiffVectorList.push_back(diffVector);
            mapIter++;
         }

         sort();
         sumDiffVectors();
      }

      ~ClientStatsDiff()
      {
         UInt64VectorListIter iter = this->opDiffVectorList.begin();
         while (iter != this->opDiffVectorList.end())
         {
            UInt64Vector* diffVec = *iter;
            iter++;
            delete diffVec;

         }
      }

   private:
      UInt64Vector* newDiffVector(uint64_t IP);
      void sort();
      void sumDiffVectors();


   public:

      /**
       * return the diff-vector-list pointer
       */
      UInt64VectorList* getDiffVectorList()
      {
         return &this->opDiffVectorList;
      }

      /**
       * Return the summary vector pointer
       */
      UInt64Vector* getSumVector()
      {
         return &this->sumVec;
      }

};

#endif /* CLIENTSTATSDIFF_H_ */
