#ifndef HIGHRESOLUTIONSTATS_H_
#define HIGHRESOLUTIONSTATS_H_

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

/**
 * Note: It is important to have the subvalues grouped, because the WorkQ needs to reset only a
 * part of the values after each reset.
 */
struct HighResolutionStats
{
   // raw values (just copied at the end of each query interval)
   struct RawVals
   {
      uint64_t statsTimeMS    = 0; // set by the StatsCollector (result of TimeAbs.getTimeMS() )

      unsigned busyWorkers    = 0; // busy workers at the time of a stats reset (updated by WorkQ)
      unsigned queuedRequests = 0; // queued requests at the time of a stats reset
                                   //  (updated by WorkQ)
   } rawVals;

   // incremental values (added up after each request until end of query interval)
   struct IncrementalVals
   {
      uint64_t diskWriteBytes = 0; // ususally updated during processing of work
      uint64_t diskReadBytes  = 0; // ususally updated during processing of work
      uint64_t netSendBytes   = 0; // ususally updated by a socket
      uint64_t netRecvBytes   = 0; // ususally updated by a socket
      unsigned workRequests   = 0; // finished work requests; usually updated by a worker
   } incVals;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->rawVals.statsTimeMS
         % obj->rawVals.busyWorkers
         % obj->rawVals.queuedRequests
         % obj->incVals.diskWriteBytes
         % obj->incVals.diskReadBytes
         % obj->incVals.netSendBytes
         % obj->incVals.netRecvBytes
         % obj->incVals.workRequests;
   }
};

template<>
struct ListSerializationHasLength<HighResolutionStats> : boost::false_type {};

typedef std::list<HighResolutionStats> HighResStatsList;
typedef HighResStatsList::iterator HighResStatsListIter;
typedef HighResStatsList::reverse_iterator HighResStatsListRevIter;

typedef std::vector<HighResolutionStats> HighResStatsVec;


class HighResolutionStatsTk
{
   private:
      HighResolutionStatsTk() {}


   public:
      // inliners

      /**
       * Adds new incremental stats to existing stats.
       *
       * Note: This is typically called after a finished work request.
       */
      static void addHighResIncStats(HighResolutionStats& newStats,
         HighResolutionStats& outUpdatedStats)
      {
         outUpdatedStats.incVals.diskWriteBytes += newStats.incVals.diskWriteBytes;
         outUpdatedStats.incVals.diskReadBytes += newStats.incVals.diskReadBytes;
         outUpdatedStats.incVals.netSendBytes += newStats.incVals.netSendBytes;
         outUpdatedStats.incVals.netRecvBytes += newStats.incVals.netRecvBytes;
         outUpdatedStats.incVals.workRequests += newStats.incVals.workRequests;
      }

      /**
       * Adds new raw stats to existing stats.
       *
       * Note: This is typically called when you add up stats for multiple nodes, so the
       * statsTimeMS value is not added here.
       */
      static void addHighResRawStats(HighResolutionStats& newStats,
         HighResolutionStats& outUpdatedStats)
      {
         outUpdatedStats.rawVals.busyWorkers += newStats.rawVals.busyWorkers;
         outUpdatedStats.rawVals.queuedRequests += newStats.rawVals.queuedRequests;
      }

      static void resetStats(HighResolutionStats* stats)
      {
         *stats = {};
      }

      static void resetIncStats(HighResolutionStats* stats)
      {
         stats->incVals = {};
      }
};

#endif /*HIGHRESOLUTIONSTATS_H_*/
