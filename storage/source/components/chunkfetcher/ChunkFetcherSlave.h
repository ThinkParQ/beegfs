#ifndef CHUNKFETCHERSLAVE_H_
#define CHUNKFETCHERSLAVE_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/fsck/FsckChunk.h>
#include <common/threading/PThread.h>

#include <mutex>

class ChunkFetcher; //forward decl.

/**
 * This component runs over all chunks of one target and gathers information suitable for fsck
 *
 * This component is not auto-started when the app starts. It is started and stopped by the
 * ChunkFetcher.
 */
class ChunkFetcherSlave : public PThread
{
   friend class ChunkFetcher; // (to grant access to internal mutex)

   public:
   ChunkFetcherSlave(uint16_t targetID);
   virtual ~ChunkFetcherSlave();

   private:
      LogContext log;

      Mutex statusMutex; // protects isRunning
      Condition isRunningChangeCond;

      bool isRunning; // true if an instance of this component is currently running

      uint16_t targetID;

      virtual void run();

   public:
      // getters & setters
      bool getIsRunning(bool isRunning)
      {
         const std::lock_guard<Mutex> lock(statusMutex);

         return this->isRunning;
      }

   private:
      void walkAllChunks();

      bool walkChunkPath(const std::string& path, uint16_t buddyGroupID, unsigned basePathLen);

      // getters & setters

      void setIsRunning(bool isRunning)
      {
         const std::lock_guard<Mutex> lock(statusMutex);

         this->isRunning = isRunning;
         isRunningChangeCond.broadcast();
      }
};

#endif /* CHUNKFETCHERSLAVE_H_ */
