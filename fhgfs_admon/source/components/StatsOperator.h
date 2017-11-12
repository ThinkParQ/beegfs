#ifndef STATSOPERATOR_H_
#define STATSOPERATOR_H_


#include <common/Common.h>
#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/threading/RWLock.h>
#include <components/requestor/DataRequestorStats.h>
#include <toolkit/stats/StatsAdmon.h>



class StatsOperator
{
   private:
      LogContext log;
      DataRequestorStatsMap requestorMap;    // synchronized by requestorMapRWLock

      RWLock requestorMapRWLock;                   // RWLock for requestorMap and lastID

      uint32_t lastID;                             // synchronized by requestorMapRWLock

      uint32_t getNextRequestorIDUnlocked();
      uint32_t getNextRequestorID();

   protected:


   public:
      StatsOperator() :
         log("StatsOperator"), lastID(0) {};

      uint32_t addOrUpdate(uint32_t requestorID, unsigned intervalSecs, unsigned maxLines,
         NodeType nodeType, bool perUser);
      StatsAdmon* getStats(uint32_t requestorID, uint64_t &outDataSequenceID);
      bool needUpdate(unsigned requestorID, unsigned intervalSecs, unsigned maxLines,
         NodeType nodeType, bool perUser);
      void cleanupStoppedRequestors();

      void shutdown()
      {
         for (DataRequestorStatsMapIter iter = requestorMap.begin();
            iter != requestorMap.end(); iter++)
         {
            iter->second->shutdown();
         }
      }

      void waitForShutdown()
      {
         for (DataRequestorStatsMapIter iter = requestorMap.begin();
            iter != requestorMap.end(); iter++)
         {
            iter->second->waitForShutdown();
         }
      }
};

#endif /* STATSOPERATOR_H_ */
