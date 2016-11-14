#ifndef CLIENTSTATS_H_
#define CLIENTSTATS_H_

#include <common/Common.h>
#include <common/nodes/Node.h>
#include <common/nodes/NodeOpStats.h>
#include <common/nodes/OpCounterTypes.h>
#include <common/toolkit/MapTk.h>

#include "CfgClientStatsOptions.h"

// We will use it to store a the vector (pointer) of op counter in a map, as we also use a list
// for easy sorting of the vectors, the IP is included into the vector.
// The map is primarily used to find the vector belonging to an IP
typedef std::map<uint64_t, UInt64Vector*> ClientNodeOpVectorMap; // int IP, Op vector pointer
typedef ClientNodeOpVectorMap::iterator ClientNodeOpVectorMapIter;

// The list is primarily used for sorting of of vectors.
typedef std::list<UInt64Vector*> UInt64VectorList;
typedef UInt64VectorList::iterator UInt64VectorListIter;


class ClientStats
{
   public:

      ClientStats(CfgClientStatsOptions *cfgOptions)
      {
         this->cfgOptions = cfgOptions;

         this->firstTransfer = true;
         this->firstRequest = true;
         this->moreDataVal = 0;
         this->cookieIP = 0;
      }

      ClientStats(ClientStats *stats)
      {
         this->cfgOptions = stats->cfgOptions;

         this->numOps = stats->numOps;
         this->firstTransfer = stats->firstTransfer;
         this->firstRequest = stats->firstRequest;
         this->moreDataVal = stats->moreDataVal;
         this->cookieIP = stats->cookieIP;

         MapTk::copyUInt64VectorMap(stats->oldVectorMap, this->oldVectorMap);
         MapTk::copyUInt64VectorMap(stats->currentVectorMap, this->currentVectorMap);
      }

      ~ClientStats()
      {
         this->emptyVectorMap(&this->oldVectorMap, true);
         this->emptyVectorMap(&this->currentVectorMap, true);
      }

      bool parseStatsVector(UInt64Vector* newVec);
      void printStats(uint64_t elapsedSecs);
      void currentToOldVectorMap(void);
      static UInt64Vector *getVectorFromMap(ClientNodeOpVectorMap *vectorMap, uint64_t IP);


   private:

      uint64_t numOps; // number of operations per IP
      int moreDataVal; // server wants to send more data (quasi-boolean)
      uint64_t cookieIP; // cookieIP for transfers, zero before first and after last transfer

      bool firstTransfer; // Do we ask the server the first time for data, or with cookieIP set?
      bool firstRequest;  // Is this the first request for data at all? So no difference data yet?

      StorageOpToStringMapping storageOpNumToStr;
      MetaOpToStringMapping metaOpNumToStr;

      void newStatsData(UInt64VectorIter vecStart, UInt64VectorIter vecEnd, uint64_t nElem);
      bool statsAdd(UInt64Vector* serverVec);
      bool emptyVectorMap(ClientNodeOpVectorMap *vectorMap, bool doDelete);
      bool addToCurrentVectorMap(UInt64Vector* vec);
      void printCounters(UInt64Vector* statsVec, uint64_t elapsedSecs);


   protected:

      CfgClientStatsOptions* cfgOptions;

      ClientNodeOpVectorMap oldVectorMap;     // previous per-client stats
      ClientNodeOpVectorMap currentVectorMap; // per-Client stats just comming in

      bool checkPrintIPStats(UInt64Vector* statsVec);
      std::string opNumToStr(int opNum);
      bool getIsRWBytesOp(int opNum) const;
      std::string valueToCfgUnit(int opNum, uint64_t inValue, double* outValue);


   public:

      /**
       * Return the the cookieIP
       */
      uint64_t getCookieIP()
      {
         return this->cookieIP;
      }

      const CfgClientStatsOptions* getCfgOptions() const
      {
         return this->cfgOptions;
      }

      /**
       * Does the server have more data?
       */
      bool moreData()
      {
         if (this->moreDataVal)
            return true;

         return false;
      }

      /**
       * All data from the server have been transferred, so reset it for next request / next server
       */
      void resetFirstTransfer()
      {
         this->firstTransfer = true;
      }


};

#endif // CLIENTSTATS_H_
