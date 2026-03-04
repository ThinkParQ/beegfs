#pragma once

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

enum ChunkBalancerJobState
{
   ChunkBalancerJobState_NOTSTARTED = 0,
   ChunkBalancerJobState_STARTING,
   ChunkBalancerJobState_RUNNING,
   ChunkBalancerJobState_SUCCESS,
   ChunkBalancerJobState_INTERRUPTED,
   ChunkBalancerJobState_FAILURE,
   ChunkBalancerJobState_ERRORS,
   ChunkBalancerJobState_IDLE
};

struct ChunkBalancerJobStatistics
{
   ChunkBalancerJobState status = ChunkBalancerJobState_NOTSTARTED;
   int64_t startTime = 0;
   int64_t endTime = 0;
   uint64_t workQueue = 0;
   uint64_t errorCount = 0;
   uint64_t lockedInodes = 0;
   uint64_t migratedChunks = 0;
   uint64_t workerNum = 0;


   void reset()
   {
      startTime = time(NULL);
      endTime = 0;
      migratedChunks = 0;
      errorCount = 0;
      workQueue = 0;
      lockedInodes = 0;
      workerNum = 0;
   }

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % serdes::as<int32_t>(obj->status)
         % obj->startTime
         % obj->endTime
         % obj->migratedChunks
         % obj->errorCount
         % obj->workQueue
         % obj->lockedInodes
         % obj->workerNum;
   }
};