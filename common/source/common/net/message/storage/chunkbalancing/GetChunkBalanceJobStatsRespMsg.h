#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/chunkbalancer/ChunkBalancerJobStatistics.h>

class GetChunkBalanceJobStatsRespMsg : public NetMessageSerdes<GetChunkBalanceJobStatsRespMsg>
{
   public:
      GetChunkBalanceJobStatsRespMsg(ChunkBalancerJobStatistics* jobStats) :
         BaseType(NETMSGTYPE_GetChunkBalanceJobStatsResp),
         jobStatsPtr(jobStats)
      {}
       //default constructor,used for deserialization
      GetChunkBalanceJobStatsRespMsg() : BaseType(NETMSGTYPE_GetChunkBalanceJobStatsResp) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
        ctx % serdes::backedPtr(obj->jobStatsPtr, obj->jobStats);
      }

   private:
      ChunkBalancerJobStatistics* jobStatsPtr;
      ChunkBalancerJobStatistics jobStats;

   public:
      ChunkBalancerJobStatistics* getJobStats()
      {
        return &jobStats;
      }

      void getJobStats(ChunkBalancerJobStatistics& outJobStats)
      {
        outJobStats = jobStats;
      }
};

