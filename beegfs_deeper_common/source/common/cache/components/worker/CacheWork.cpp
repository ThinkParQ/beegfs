#include "CacheWork.h"



/**
 * Checks if the current work should be aborted.
 *
 * NOTE: We doesn't use the mutex, because this function is performance critical. It is a read-only
 *       access to the state variable and in the worst case we need a additional iteration (read and
 *       write of buffer) before aborting a flush or prefetch.
 *
 * @return True if the work should be aborted.
 */
bool CacheWork::shouldStop()
{
   if(rootWork && rootWork->shouldStop() )
      return true;

   return CACHEWORKSTATE_SHOULD_STOP(state);
}

/**
 * Changes the state to stopping or the currently required state to abort the work.
 */
void CacheWork::stopWork()
{
   SafeMutexLock safeLock(&lockState);

   switch(state)
   {
      case CacheWorkState_INIT:
         state = CacheWorkState_STOPPING;
         break;
      case CacheWorkState_RUNNING:
         state = CacheWorkState_STOPPING;
         break;
      case CacheWorkState_STOPPING:
         break;
      case CacheWorkState_WAIT_FOR_CHILDREN:
         state = CacheWorkState_STOPPING_WAIT_FOR_CHILDREN;
         break;
      case CacheWorkState_STOPPING_WAIT_FOR_CHILDREN:
         break;
      case CacheWorkState_FINISHED:
         if(childCount)
            state = CacheWorkState_STOPPING_WAIT_FOR_CHILDREN;
         break;
      default:
         break;
   }

   safeLock.unlock();
}

/**
 * Processing function of the work which is called by the worker.
 *
 * NOTE: Implement the hook function doProcess in all bas classes because some init and cleanup
 *       stuff is required which is implemented in this function.
 *
 * @param bufIn Input buffer.
 * @param bufInLen Length of the input buffer.
 * @param bufOut Output buffer.
 * @param bufOutLen Length of the output buffer.
 */
void CacheWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   bool shouldStart = startWork();

   if(shouldStart)
      doProcess(bufIn, bufInLen, bufOut, bufOutLen);

   finishWork();
}

/**
 * Converts a CacheWorkType into a string, useful for logging.
 *
 * @param type The CacheWorkType to convert into a string.
 * @return The string representation of the CacheWorkType.
 */
std::string CacheWork::cacheWorkTypeToString(CacheWorkType type)
{
   if(type == CacheWorkType_NONE)
      return "CacheWorkType_NONE";
   else if(type == CacheWorkType_FLUSH)
      return "CacheWorkType_FLUSH";
   else if(type == CacheWorkType_PREFETCH)
      return "CacheWorkType_PREFETCH";
   else if(type == CacheWorkType_DISCARD)
      return "CacheWorkType_DISCARD";
   else
      return "CacheWorkType_UNKNOWN";
}

/**
 * Getter for the children count of the root work, if this function is not the root work the
 * call is forwarded to the root work.
 *
 * @return The number of the children.
 */
uint64_t CacheWork::getChildCount()
{
   if(rootWork)
   {
      return rootWork->getChildCount();
   }
   else
   {
      uint64_t count;

      SafeMutexLock safeLock(&lockState);

      count = childCount;

      safeLock.unlock();

      return count;
   }
}

/**
 * Getter for the state of the work.
 *
 * @return The state a value from the enum CacheWorkState.
 */
CacheWorkState CacheWork::getState()
{
   CacheWorkState workState;

   SafeMutexLock safeLock(&lockState);

   workState = state;

   safeLock.unlock();

   return workState;
}

/**
 * Updates the result of this work and if an error happened the root work is also updated.
 *
 * return True if the result could be updated, if the result value contains an error before
 *        this update the function returns false.
 */
bool CacheWork::updateResult(int error)
{
   if(!error) // noting to do because, no error to report
      return true;

   // only update if no error happened before, only relevant on the root work because every
   // child could report an error, a child has only its own result
   bool retVal = result.compareAndSet(error, 0);

   // update the result of the root work
   if(rootWork)
      retVal = rootWork->updateResult(error);

   // update the result of the split root work
   if(splitRootWork)
      retVal = splitRootWork->updateResult(error);

   return retVal;
}

/**
 * Prints a readable string from a CacheWorkState.
 *
 * @param state The state to print.
 */
std::string CacheWork::cacheWorkStateToString(CacheWorkState state)
{
   switch(state)
   {
      case CacheWorkState_INIT:
         return "CacheWorkState_INIT";
         break;
      case CacheWorkState_RUNNING:
         return "CacheWorkState_RUNNING";
         break;
      case CacheWorkState_STOPPING:
         return "CacheWorkState_STOPPING";
         break;
      case CacheWorkState_WAIT_FOR_CHILDREN:
         return "CacheWorkState_WAIT_FOR_CHILDREN";
         break;
      case CacheWorkState_STOPPING_WAIT_FOR_CHILDREN:
         return "CacheWorkState_STOPPING_WAIT_FOR_CHILDREN";
         break;
      case CacheWorkState_FINISHED:
         return "CacheWorkState_FINISHED";
         break;
      default:
         return "CacheWorkState_FINISHED";
         break;
   }
}

/**
 * Sets the state to finished.
 */
void CacheWork::setStateToFinished()
{
   SafeMutexLock safeLock(&lockState);

   setStateToFinishedUnlocked();

   safeLock.unlock();
}

/**
 * Sets the state to finished.
 *
 * NOTE: It doesn't lock the state lock. The state lock must be locked before.
 */
void CacheWork::setStateToFinishedUnlocked()
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Change state of work to finished: " + sourcePath);
#endif
   state = CacheWorkState_FINISHED;
}

/**
 * Decreased the child counter if this object is the root work else it is forwarded to the
 * root work.
 *
 * NOTE: It doesn't lock the state lock. The state lock must be locked before.
 *
 * @return The previous value of the child counter.
 */
uint64_t CacheWork::decreaseChildCountUnlocked()
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Decrease: " + sourcePath + "; counter: " + StringTk::uint64ToStr(childCount -1) );
#endif
   return --childCount;
}

/**
 * Increased the child counter if this object is the root work else it is forwarded to the
 * root work.
 *
 * NOTE: It doesn't lock the state lock. The state lock must be locked before.
 *
 * @return The previous value of the child counter.
 */
uint64_t CacheWork::increaseChildCountUnlocked()
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Increase: " + sourcePath + "; counter: " + StringTk::uint64ToStr(childCount +1) +
      "; is root work: " + StringTk::intToStr(rootWork ? false : true) +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif
   return ++childCount;
}

/**
 * Decreased the child counter if this object is the root work else it is forwarded to the
 * root work.
 *
 * @return The value of the child counter.
 */
uint64_t CacheWork::decreaseChildCount()
{
   if(rootWork)
   {
      return rootWork->decreaseChildCount();
   }
   else
   {
      uint64_t count;

      SafeMutexLock safeLock(&lockState);

      count = decreaseChildCountUnlocked();

      safeLock.unlock();

      return count;
   }
}

/**
 * Increased the child counter if this object is the root work else it is forwarded to the
 * root work.
 *
 * @return The value of the child counter.
 */
uint64_t CacheWork::increaseChildCount(bool rootWorkIsLocked)
{
   if(rootWork)
   {
      if(rootWorkIsLocked)
      {
         return rootWork->increaseChildCountUnlocked();
      }
      else
      {
         return rootWork->increaseChildCount(rootWorkIsLocked);
      }
   }
   else
   {
      uint64_t count;

      SafeMutexLock safeLock(&lockState);

      count = increaseChildCountUnlocked();

      safeLock.unlock();

      return count;
   }
}

/**
 * Decreased the child counter if this object is the root work else it is forwarded to the
 * root work.
 *
 * NOTE: It doesn't lock the state lock. The state lock must be locked before.
 *
 * @return The previous value of the child counter.
 */
uint64_t CacheWork::decreaseSplitCountUnlocked()
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Decrease: " + sourcePath + "; counter: " + StringTk::uint64ToStr(splitCount -1) +
      "; is root work: " + StringTk::intToStr(rootWork ? false : true) +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif
   return --splitCount;
}

/**
 * Increased the child counter if this object is the root work else it is forwarded to the
 * root work.
 *
 * NOTE: It doesn't lock the state lock. The state lock must be locked before.
 *
 * @return The previous value of the child counter.
 */
uint64_t CacheWork::increaseSplitCountUnlocked()
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Increase: " + sourcePath + "; counter: " + StringTk::uint64ToStr(splitCount +1) );
#endif
   return ++splitCount;
}

/**
 * Decreased the child counter if this object is the root work else it is forwarded to the
 * root work.
 *
 * @return The value of the child counter.
 */
uint64_t CacheWork::decreaseSplitCount()
{
   if(splitRootWork)
   {
      return splitRootWork->decreaseSplitCount();
   }
   else
   {
      uint64_t count;

      SafeMutexLock safeLock(&lockState);

      count = decreaseSplitCountUnlocked();

      safeLock.unlock();

      return count;
   }
}

/**
 * Increased the child counter if this object is the root work else it is forwarded to the
 * root work.
 *
 * @return The value of the child counter.
 */
uint64_t CacheWork::increaseSplitCount()
{
   if(splitRootWork)
   {
      return splitRootWork->increaseSplitCount();
   }
   else
   {
      uint64_t count;

      SafeMutexLock safeLock(&lockState);

      count = increaseSplitCountUnlocked();

      safeLock.unlock();

      return count;
   }
}

/**
 * Changes a work into a split work.
 *
 * NOTE: This is only allowed for ranged flushes or prefetches.
 *
 * @param newSplitRootWork The split root work.
 * @return True the work could be changed into a split work. False if the work was a split
 *         work and couldn't be changed.
 */
bool CacheWork::changeToSplitWork(CacheWork* newSplitRootWork)
{
   if(splitRootWork)
   {
      return false;
   }
   else
   {
      SafeMutexLock safeLock(&lockState);

      splitRootWork = newSplitRootWork;

      safeLock.unlock();

      return true;
   }
}

/**
 * Adds a split work to this work. Increments the counter and checks the state.
 *
 * NOTE: This is only allowed for ranged flushes or prefetches.
 */
void CacheWork::addSplitWork()
{
   SafeMutexLock safeLock(&lockState);

   increaseSplitCountUnlocked();

   if(state == CacheWorkState_FINISHED)
   {
      state = CacheWorkState_WAIT_FOR_CHILDREN;
   }

   safeLock.unlock();
}
