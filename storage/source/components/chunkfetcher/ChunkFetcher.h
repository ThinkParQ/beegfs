#ifndef CHUNKFETCHER_H_
#define CHUNKFETCHER_H_

#include <components/chunkfetcher/ChunkFetcherSlave.h>
#include <common/toolkit/ListTk.h>

#include <mutex>

#define MAX_CHUNKLIST_SIZE 5000

// forward declaration
class ChunkFetcher;

typedef std::list<ChunkFetcherSlave> ChunkFetcherSlaveList;
typedef ChunkFetcherSlaveList::iterator ChunkFetcherSlaveListIter;

/**
 * This is not a component that represents a separate thread. Instead, it contains and controls
 * slave threads, which are started and stopped on request (i.e. they are not automatically started
 * when the app is started).
 * The slave threads will run over all chunks on all targets and read them in a format suitable for
 * fsck
 */
class ChunkFetcher
{
   public:
      ChunkFetcher();
      virtual ~ChunkFetcher();

      bool startFetching();
      void stopFetching();
      void waitForStopFetching();

   private:
      LogContext log;
      ChunkFetcherSlaveList slaves;

      FsckChunkList chunksList;
      Mutex chunksListMutex;
      Condition chunksListFetchedCondition;
      bool isBad;

   public:
      bool getIsBad()
      {
         const std::lock_guard<Mutex> lock(chunksListMutex);

         return isBad;
      }

      void setBad()
      {
         const std::lock_guard<Mutex> lock(chunksListMutex);

         isBad = true;
      }

      void addChunk(FsckChunk& chunk)
      {
         const std::lock_guard<Mutex> lock(chunksListMutex);

         if (chunksList.size() > MAX_CHUNKLIST_SIZE)
            chunksListFetchedCondition.wait(&chunksListMutex);

         chunksList.push_back(chunk);
      }

      bool isQueueEmpty()
      {
         std::lock_guard<Mutex> lock(chunksListMutex);
         return chunksList.empty();
      }


      void getAndDeleteChunks(FsckChunkList& outList, unsigned numChunks)
      {
         const std::lock_guard<Mutex> lock(chunksListMutex);

         FsckChunkListIter iterEnd = this->chunksList.begin();
         ListTk::advance(this->chunksList, iterEnd, numChunks);

         outList.splice(outList.end(), this->chunksList, this->chunksList.begin(), iterEnd);

         chunksListFetchedCondition.signal();
      }

      unsigned getNumRunning()
      {
         unsigned retVal = 0;

         for (ChunkFetcherSlaveListIter iter = slaves.begin(); iter != slaves.end(); iter++)
         {
            const std::lock_guard<Mutex> lock(iter->statusMutex);

            if ( iter->isRunning )
               retVal++;
         }

         return retVal;
      }
};

#endif /* CHUNKFETCHER_H_ */
