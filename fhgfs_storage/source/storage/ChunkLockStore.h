#ifndef CHUNKLOCKSTORE_H_
#define CHUNKLOCKSTORE_H_

#include <common/Common.h>
#include <common/threading/Condition.h>
#include <common/threading/Mutex.h>
#include <common/threading/RWLock.h>
#include <common/threading/SafeMutexLock.h>

struct ChunkLockStoreContents
{
   StringSet lockedChunks;
   Mutex lockedChunksMutex;
   Condition chunkUnlockedCondition;
};

/**
 * Can be used to lock access to a chunk, especially needed for resync; locks will be on a very
 * high, abstract level, as we do only restrict access by chunk ID.
 *
 * "locking" here means we successfully insert an element into a set. If we cannot insert, because
 * an element with the same ID is already present, it means someone else currently holds the lock.
 */
class ChunkLockStore
{
   friend class GenericDebugMsgEx;

   public:
      ChunkLockStore() { };

   private:
      std::map<uint16_t, std::shared_ptr<ChunkLockStoreContents>> targetsMap;
      RWLock targetsLock; // synchronizes insertion into targetsMap

   public:
      void lockChunk(uint16_t targetID, std::string chunkID)
      {
         targetsLock.readLock(); // lock targets map

         ChunkLockStoreContents* targetLockStore = nullptr;

         auto targetsIter = targetsMap.find(targetID);
         if (targetsIter != targetsMap.end())
         {
            targetLockStore = targetsIter->second.get();
         }
         else
         { // slow path: get write lock and insert
            targetsLock.unlock();
            targetsLock.writeLock();

            targetsMap.insert({targetID, std::make_shared<ChunkLockStoreContents>()});
            targetLockStore = targetsMap[targetID].get();
         }

         SafeMutexLock chunksLock(&targetLockStore->lockedChunksMutex);

         // loop until we can insert the chunk lock
         for( ; ; )
         {
            bool insertRes = targetLockStore->lockedChunks.insert(chunkID).second;

            if(insertRes)
               break; // new lock successfully inserted

            // chunk lock already exists => wait
            targetLockStore->chunkUnlockedCondition.wait(&targetLockStore->lockedChunksMutex);
         }

         chunksLock.unlock();

         targetsLock.unlock(); // unlock targets map
      }

      void unlockChunk(uint16_t targetID, std::string chunkID)
      {
         ChunkLockStoreContents* targetLockStore;
         StringSetIter lockChunksIter;

         targetsLock.readLock(); // lock targets map

         auto targetsIter = targetsMap.find(targetID);
         if(unlikely(targetsIter == targetsMap.end() ) )
         { // target not found should never happen here
            LogContext(__func__).log(Log_WARNING,
               "Tried to unlock chunk, but target not found in lock map. Printing backtrace. "
               "targetID: " + StringTk::uintToStr(targetID) );
            LogContext(__func__).logBacktrace();

            goto unlock_targets;
         }

         targetLockStore = targetsIter->second.get();

         targetLockStore->lockedChunksMutex.lock();

         lockChunksIter = targetLockStore->lockedChunks.find(chunkID);
         if(unlikely(lockChunksIter == targetLockStore->lockedChunks.end() ) )
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to unlock chunk, but chunk not found in lock set. Printing backtrace. "
               "targetID: " + StringTk::uintToStr(targetID) + "; "
               "chunkID: " + chunkID);
            LogContext(__func__).logBacktrace();

            goto unlock_chunks;
         }

         targetLockStore->lockedChunks.erase(lockChunksIter);

         targetLockStore->chunkUnlockedCondition.broadcast(); // notify lock waiters

         unlock_chunks:
            targetLockStore->lockedChunksMutex.unlock();

         unlock_targets:
            targetsLock.unlock(); // unlock targets map
      }

   protected:
      // only used for GenericDebugMsg at the moment
      size_t getSize(uint16_t targetID)
      {
         size_t retVal = 0;

         targetsLock.readLock(); // lock targets map

         auto targetsIter = targetsMap.find(targetID);
         if(targetsIter != targetsMap.end() ) // map does exist
         {
            ChunkLockStoreContents* targetLockStore = targetsIter->second.get();

            SafeMutexLock chunksLock(&targetLockStore->lockedChunksMutex);

            retVal = targetLockStore->lockedChunks.size();

            chunksLock.unlock();
         }

         targetsLock.unlock(); // unlock targets map

         return retVal;
      }

      StringSet getLockStoreCopy(uint16_t targetID)
      {
         StringSet outLockStore;

         targetsLock.readLock(); // lock targets map

         auto targetsIter = targetsMap.find(targetID);
         if(targetsIter != targetsMap.end() ) // map does exist
         {
            ChunkLockStoreContents* targetLockStore = targetsIter->second.get();

            SafeMutexLock chunksLock(&targetLockStore->lockedChunksMutex);

            outLockStore = targetLockStore->lockedChunks;

            chunksLock.unlock();
         }

         targetsLock.unlock(); // unlock targets map

         return outLockStore;
      }
};

#endif /*CHUNKLOCKSTORE_H_*/
