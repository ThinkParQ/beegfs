#ifndef CHUNKLOCKSTORE_H_
#define CHUNKLOCKSTORE_H_

#include <common/Common.h>
#include <common/threading/Condition.h>
#include <common/threading/Mutex.h>
#include <common/threading/UniqueRWLock.h>
#include <common/threading/RWLock.h>

#include <mutex>

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

      ChunkLockStoreContents* getOrInsertTargetLockStore(uint16_t targetID)
      {
         UniqueRWLock lock(targetsLock, SafeRWLock_READ);

         auto targetsIter = targetsMap.find(targetID);
         if (targetsIter != targetsMap.end())
         {
            return targetsIter->second.get();
         }
         else
         {
            lock.unlock();
            lock.lock(SafeRWLock_WRITE);

            return targetsMap.insert({targetID, std::make_shared<ChunkLockStoreContents>()})
               .first->second.get();
         }
      }

      ChunkLockStoreContents* findTargetLockStore(uint16_t targetID)
      {
         UniqueRWLock lock(targetsLock, SafeRWLock_READ);
         auto targetsIter = targetsMap.find(targetID);

         if (unlikely(targetsIter == targetsMap.end()))
         {
            LogContext(__func__).log(Log_WARNING,
               "Tried to access chunk lock, but target not found in lock map. Printing backtrace. "
               "targetID: " + StringTk::uintToStr(targetID) );
            LogContext(__func__).logBacktrace();

            return nullptr;
         }

         return targetsIter->second.get();
      }

   public:
      void lockChunk(uint16_t targetID, std::string chunkID)
      {
         auto targetLockStore = getOrInsertTargetLockStore(targetID);

         const std::lock_guard<Mutex> chunksLock(targetLockStore->lockedChunksMutex);

         // loop until we can insert the chunk lock
         for( ; ; )
         {
            bool insertRes = targetLockStore->lockedChunks.insert(chunkID).second;

            if(insertRes)
               break; // new lock successfully inserted

            // chunk lock already exists => wait
            targetLockStore->chunkUnlockedCondition.wait(&targetLockStore->lockedChunksMutex);
         }
      }

      void unlockChunk(uint16_t targetID, std::string chunkID)
      {
         StringSetIter lockChunksIter;

         auto targetLockStore = findTargetLockStore(targetID);
         if(unlikely(targetLockStore == nullptr))
            return;

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
      }

   protected:
      // only used for GenericDebugMsg at the moment
      size_t getSize(uint16_t targetID)
      {
         size_t retVal = 0;

         auto targetLockStore = findTargetLockStore(targetID);
         if(targetLockStore) // map does exist
         {
            const std::lock_guard<Mutex> chunksLock(targetLockStore->lockedChunksMutex);

            retVal = targetLockStore->lockedChunks.size();
         }

         return retVal;
      }

      StringSet getLockStoreCopy(uint16_t targetID)
      {
         StringSet outLockStore;

         auto targetLockStore = findTargetLockStore(targetID);
         if(targetLockStore) // map does exist
         {
            const std::lock_guard<Mutex> chunksLock(targetLockStore->lockedChunksMutex);

            outLockStore = targetLockStore->lockedChunks;
         }

         return outLockStore;
      }
};

#endif /*CHUNKLOCKSTORE_H_*/
