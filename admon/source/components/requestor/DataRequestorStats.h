#ifndef DATAREQUESTORSTATS_H_
#define DATAREQUESTORSTATS_H_


#include <common/app/log/LogContext.h>
#include <clientstats/CfgClientStatsOptions.h>
#include <clientstats/ClientStats.h>
#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/threading/Mutex.h>
#include <common/threading/PThread.h>
#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/TimeAbs.h>
#include <toolkit/stats/StatsAdmon.h>


class DataRequestorStats;


typedef std::map<unsigned, DataRequestorStats*> DataRequestorStatsMap;
typedef DataRequestorStatsMap::iterator DataRequestorStatsMapIter;
typedef DataRequestorStatsMap::const_iterator DataRequestorStatsMapConstIter;
typedef DataRequestorStatsMap::value_type DataRequestorStatsMapVal;


/*
 * enum for the states of the state machine of the storage benchmark operator
 * note: see STORAGEBENCHSTATUS_IS_ACTIVE
 */
enum DataRequestorStatsStatus
{
   DataRequestorStatsStatus_INITIALIZED = 0,
   DataRequestorStatsStatus_RUNNING = 1,
   DataRequestorStatsStatus_STOPPING = 2,
   DataRequestorStatsStatus_STOPPED = 3
};

#define DATAREQUESTORSTATSSTATUS_IS_ACTIVE(status) \
   ( (status == DataRequestorStatsStatus_RUNNING) || \
   (status == DataRequestorStatsStatus_STOPPING) )


/*
 * The data requestor is a PThread, which periodically polls the nodes for data to display. It
 * creates work packages (which send messages to the nodes and take care of the response) in fixed
 * intervals. The implementation for the client stats.
 */
class DataRequestorStats : public PThread
{
   public:
      DataRequestorStats(unsigned id, unsigned intervalSecs, unsigned maxLines,
         NodeType nodeType, bool perUser)
          : PThread("DataReq-Stats-" + StringTk::uintToStr(id) ),
            log("DataRwq-Stats"),
            id(id),
            cfgOptions(CfgClientStatsOptions(intervalSecs, maxLines, nodeType, perUser) ),
            stats(new ClientStats(&this->cfgOptions) ),
            dataSequenceID(0),
            status(DataRequestorStatsStatus_INITIALIZED)
      {
      }

      virtual ~DataRequestorStats()
      {
         SAFE_DELETE(this->stats);
      }

   private:
      LogContext log;

      const unsigned id;
      Mutex accessDataMutex;        // synchronize the access to the members stats, dataSequenceID,
                                    // lastUpdate and lastRequest
      Mutex statusMutex;            // synchronize the access to status variable
      Condition statusChangeCond;   //

      CfgClientStatsOptions cfgOptions;   // the client stats option for this requestor
      ClientStats* stats;                 // the client stats for this requestor
      Time lastRequest;                   // timestamp of the client request to get the stats

      uint64_t dataSequenceID;            // incremented after every update, indicator for new data

      DataRequestorStatsStatus status;    // status of this requestor

      int requestLoop();
      virtual void run();

      bool setStatusToRunning();
      void setStatusToStopped();

   public:
      StatsAdmon* getStatsCopy(uint64_t &outDataSequenceID);
      void shutdown();
      void waitForShutdown();

      void stop()
      {
         SafeMutexLock lock(&statusMutex);              // L O C K

         this->status = DataRequestorStatsStatus_STOPPING;
         this->statusChangeCond.broadcast();

         lock.unlock();                                 // U N L O C K
      }

      DataRequestorStatsStatus getStatus()
      {
         DataRequestorStatsStatus retVal;

         SafeMutexLock lock(&statusMutex);              // L O C K

         retVal = this->status;

         lock.unlock();                                 // U N L O C K

         return retVal;
      }

      CfgClientStatsOptions getCfgStatsOptions()
      {
         return this->cfgOptions;
      }

      u_int64_t getDataSequenceID()
      {
         uint64_t retVal;

         SafeMutexLock lock(&this->accessDataMutex);        // L O C K

         retVal = this->dataSequenceID;

         lock.unlock();                                     // U N L O C K

         return retVal;
      }
};

#endif /* DATAREQUESTORSTATS_H_ */
