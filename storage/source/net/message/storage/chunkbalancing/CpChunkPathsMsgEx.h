#pragma once

#include <common/net/message/storage/chunkbalancing/CpChunkPathsMsg.h>

class ChunkBalancerJob;

class CpChunkPathsMsgEx : public CpChunkPathsMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
   private: 
      Mutex ChunkBalanceJobMutex;
      ChunkBalancerJob* addChunkBalanceJob();

};

