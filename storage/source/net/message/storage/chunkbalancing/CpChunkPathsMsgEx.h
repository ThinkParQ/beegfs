#pragma once

#include <common/net/message/storage/chunkbalancing/CpChunkPathsMsg.h>
#include <components/chunkbalancer/ChunkBalancerJob.h>

class CpChunkPathsMsgEx : public CpChunkPathsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
   private: 
      ChunkBalancerJob* addChunkBalanceJob(bool& outIsNew);
};
