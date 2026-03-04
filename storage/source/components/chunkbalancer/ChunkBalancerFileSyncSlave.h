#pragma once

#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/PThread.h>
#include <components/buddyresyncer/ChunkFileResyncer.h>

#include <mutex>

/**
 * A storage chunk balancing slave component. Started by the chunk balance job component.  
 * Processes the items in the queues and performs the copy chunk operation. Then sends a message to metadata to modify stripe pattern. 
 */

class ChunkBalancerFileSyncSlave : public ChunkFileResyncer
{

   friend class ChunkBalancerJob; // (to grant access to internal mutex)
   public:
      ChunkBalancerFileSyncSlave(uint16_t targetID, ChunkSyncCandidateStore* syncCandidates, uint8_t  slaveID);

      virtual ~ChunkBalancerFileSyncSlave();
   private:
      
      ChunkSyncCandidateStore* syncCandidates;
      uint16_t targetID;
      bool isBuddyMirrorChunk;
      virtual void syncLoop();
      virtual int getFD(const std::unique_ptr<StorageTarget> & target);
      FhgfsOpsErr removeChunk(int& fd, std::string& relativePath);
      bool sendRemoveChunkPathsMessage(uint16_t& targetID, std::string& relativePath, bool isBuddyMirrorChunk);
 };     

typedef std::vector<ChunkBalancerFileSyncSlave*> ChunkBalancerFileSyncSlaveVec;
typedef ChunkBalancerFileSyncSlaveVec::iterator ChunkBalancerFileSyncSlaveVecIter;


