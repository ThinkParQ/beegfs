#include <app/App.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include <toolkit/asyncfilesystemtk.h>
#include "DeeperWork.h"



/**
 * Must be called at the beginning of the process function, This is done by the CacheWork::process
 * function which also calls the virtual function doProcess which is a hook function which must be
 * implemented by every derived class.
 *
 * @return True if the processing can start, else false (stop function was called)
 */
bool DeeperWork::startWork()
{
   int retVal = true;

   SafeMutexLock safeLock(&lockState);

   if(state == CacheWorkState_INIT)
      state = CacheWorkState_RUNNING;
   else
      retVal = false;

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Start work on path: " + sourcePath +
      "; state: " + cacheWorkStateToString(state) +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#else
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, printForLog() );
#endif

   safeLock.unlock();

   return retVal;
}

/**
 * Removes the work from the is in processing map and if a error happened work is added to map with
 * work with an error result.
 *
 * Note: Must be called at the end of the process function. This is done by the CacheWork::process
 *       function which also calls the virtual function doProcess which is a hook function which
 *       must be implemented by every derived class.
 */
void DeeperWork::finishWork()
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Finishing work on path: " + sourcePath +
      "; state: " + cacheWorkStateToString(state) +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif

   App* app = Program::getApp();
   CacheWorkKey key;
   getKey(key);

   SafeMutexLock safeLock(&lockState);

   if(rootWork)
   { // child work
      if(splitCount)
      { // splits are running -> last split cleanup the root work
         state = CacheWorkState_WAIT_FOR_CHILDREN;
      }
      else
      { // child and all possible splits are finished.
         // first decrease childCount and change the state of the root work
         ((DeeperWork*)rootWork)->childFinished(this);
         // cleanup child work
         setStateToFinishedUnlocked();
         app->getWorkQueue()->handleFinishedWork(key, result.read() );
      }
   }
   else if(splitRootWork)
   { // split work
      // first decrease splitCount and change the state of the split root work
      ((DeeperWork*)splitRootWork)->splitFinished(this);

      setStateToFinishedUnlocked();
   }
   else
   { // root work or split root work or work without children
      if( (!childCount) && (!splitCount) )
      { // no children and split works are running -> cleanup itself
         setStateToFinishedUnlocked();
         app->getWorkQueue()->handleFinishedWork(key, result.read() );
      }
      else if(state == CacheWorkState_STOPPING)
      { // children are running -> last child cleanup the root work
         state = CacheWorkState_STOPPING_WAIT_FOR_CHILDREN;
      }
      else
      { // children or split is running -> last child or split cleanup the root work
         state = CacheWorkState_WAIT_FOR_CHILDREN;
      }
   }

#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
      "Work finished on path: " + sourcePath +
      "; state: " + cacheWorkStateToString(state)  +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif
   safeLock.unlock();
}

/**
 * Should only executed on a root work. Decrease the child counter. Removes the root work from the
 * is in processing map and if a error happened the work is added to map with works which has an
 * error result.
 *
 * Note: Must be called at the end of the process function. This is done by the CacheWork::process
 *       function which also calls the virtual function doProcess which is a hook function which
 *       must be implemented by every derived class.
 *
 * @param child The child work, which finished its work.
 */
void DeeperWork::childFinished(CacheWork* child)
{
   App* app = Program::getApp();
   Logger* log = Logger::getLogger();

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__,
      "Finishing Work on path: " + sourcePath +
      "; state: " + cacheWorkStateToString(state)  +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif

   CacheWorkKey key;
   getKey(key);

   SafeMutexLock safeLock(&lockState);

   uint64_t counterChildren = decreaseChildCountUnlocked();

   if( (!counterChildren) && (!splitCount) && (state != CacheWorkState_RUNNING) &&
      (state != CacheWorkState_STOPPING) )
   { // no children and split works are running and the root split work is not running any more
      if( (doDiscardForRoot == CacheWork_DISCARD_SUBDIRS) &&
         (result.read() == DEEPER_RETVAL_SUCCESS) )
      {
         if(!shouldStop() )
         {
            doDiscardForRoot = CacheWork_DISCARD_ROOTDIR;

            bool newChildrenCreated = false;
            if(asyncfilesystemtk::doDelete(sourcePath.c_str(), deeperFlags, this, true,
               newChildrenCreated) )
            {
               result.set(errno);
            }

            if(newChildrenCreated)
            { // do not finish this work, because doDelete created new children
               goto doUnlock;
            }

            // delete the root dir, because no children are running
            doDiscardForRoot = CacheWork_DISCARD_NO;
            if(rmdir(sourcePath.c_str() ) )
            {
               log->log(Log_ERR, __FUNCTION__,
                  "Could not delete directory after copy. path: " + sourcePath +
                  "; errno: " + System::getErrString(errno) );
               result.set(errno);
            }
         }

         setStateToFinishedUnlocked();
         app->getWorkQueue()->handleFinishedWork(key, result.read() );
         child->setDoMemDeleteRootUnlocked();
      }
      else if( (doDiscardForRoot == CacheWork_DISCARD_ROOTDIR) &&
         (result.read() == DEEPER_RETVAL_SUCCESS) )
      {
         doDiscardForRoot = CacheWork_DISCARD_NO;
         if(!shouldStop() )
         {
            if(rmdir(sourcePath.c_str() ) )
            {
               log->log(Log_ERR, __FUNCTION__,
                  "Could not delete directory after copy. path: " + sourcePath +
                  "; errno: " + System::getErrString(errno) );
               result.set(errno);
            }
         }
         setStateToFinishedUnlocked();
         app->getWorkQueue()->handleFinishedWork(key, result.read() );
         child->setDoMemDeleteRootUnlocked();
      }
      else
      {
         setStateToFinishedUnlocked();
         app->getWorkQueue()->handleFinishedWork(key, result.read() );
         child->setDoMemDeleteRootUnlocked();
      }
   }

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Work finished on path: " + sourcePath +
      "; state: " + cacheWorkStateToString(state)  +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif

doUnlock:
   safeLock.unlock();
}

/**
 * Should only executed on a split root work. Decrease the split counter. Removes the root work of a
 * split from the is in processing map and if a error happened the work is added to map with works
 * which has an error result.
 *
 * Note: Must be called at the end of the process function. This is done by the CacheWork::process
 *       function which also calls the virtual function doProcess which is a hook function which
 *       must be implemented by every derived class.
 *
 * @param split The child work, which finished its work.
 */
void DeeperWork::splitFinished(CacheWork* split)
{
   App* app = Program::getApp();
   Logger* log = Logger::getLogger();

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Finishing Work on path: " + sourcePath +
      "; state: " + cacheWorkStateToString(state)  +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif

   CacheWorkKey key;
   getKey(key);

   SafeMutexLock safeLock(&lockState);

   uint64_t counterSplits = decreaseSplitCountUnlocked();

   if( (!counterSplits) && (!childCount) && (state != CacheWorkState_RUNNING) &&
      (state != CacheWorkState_STOPPING) )
   { // no children and split works are running and the root split work is not running any more
      if(doDiscardForSplit && (result.read() == DEEPER_RETVAL_SUCCESS) )
      {
         if(!shouldStop() )
         {
            if(unlink(sourcePath.c_str() ) )
            {
               log->log(Log_ERR, __FUNCTION__,
                  "Could not delete file after copy. path: " + sourcePath +
                  "; errno: " + System::getErrString(errno) );
               result.set(errno);
            }
         }
      }

      if(rootWork)
      {  // child and all possible splits are finished.
         // first decrease childCount and change the state of the root work
         ((DeeperWork*)rootWork)->childFinished(this);
         // cleanup child work
         setStateToFinishedUnlocked();
         app->getWorkQueue()->handleFinishedWork(key, result.read() );
      }

      setStateToFinishedUnlocked();
      app->getWorkQueue()->handleFinishedWork(key, result.read() );
      split->setDoMemDeleteSplitUnlocked();
   }

#ifdef BEEGFS_DEBUG
   log->log(Log_DEBUG, __FUNCTION__, "Work finished on path: " + sourcePath +
      "; state: " + cacheWorkStateToString(state)  +
      "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif
   safeLock.unlock();
}
