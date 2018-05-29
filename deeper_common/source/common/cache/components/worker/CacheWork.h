#ifndef COMPONENTS_WORKER_CACHEWORK_H_
#define COMPONENTS_WORKER_CACHEWORK_H_


#include <common/app/AbstractApp.h>
#include <common/app/log/Logger.h>
#include <common/Common.h>
#include <common/components/worker/Work.h>
#include <common/threading/Atomics.h>
#include <common/threading/Condition.h>
#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/StringTk.h>

#include <boost/functional/hash.hpp>

#include <limits>
#include <unordered_map>



class CacheWork;

const int CacheWork_DISCARD_NO =        0;
const int CacheWork_DISCARD_SUBDIRS =   1;
const int CacheWork_DISCARD_ROOTDIR =   2;

/**
 * Ahe action type of the work package.
 */
enum CacheWorkType
{
   CacheWorkType_NONE = 0,
   CacheWorkType_PREFETCH = 1,
   CacheWorkType_FLUSH = 2,
   CacheWorkType_DISCARD = 3
};

/**
 * The state of the work package.
 */
enum CacheWorkState
{
   CacheWorkState_INIT = 0,                       // initial state after constructor
   CacheWorkState_RUNNING = 1,                    // state during the processing of the work
   CacheWorkState_STOPPING = 2,                   // the work and all children should be aborted
   CacheWorkState_WAIT_FOR_CHILDREN= 3,           // the work was processed but not all children
   CacheWorkState_STOPPING_WAIT_FOR_CHILDREN = 4, // the work was aborted and waits for all children
   CacheWorkState_FINISHED = 5                    // the work was processed and all children
};

#define CACHEWORKSTATE_SHOULD_STOP(state) ( (state == CacheWorkState_STOPPING) || \
   (state == CacheWorkState_STOPPING_WAIT_FOR_CHILDREN) )

/**
 * The key for every hash map which should identify a cache work package.
 */
struct CacheWorkKey
{
   std::string sourcePath;       // the source path of the work
   std::string destPath;         // the source path of the work
   CacheWorkType type;           // the type of the work, a value of the enum CacheWorkType

   /**
    * Equals operator.
    */
   bool operator==(const CacheWorkKey& key2) const
   {
      return (type == key2.type) && (sourcePath == key2.sourcePath) && (destPath == key2.destPath);
   }
};

/**
 * Hash generator for a CacheWorkKey, this hash will be used as key for the hash maps.
 *
 * @param key The CacheWorkKey which needs to be hashed.
 * @return The hash of the CacheWorkKey.
 */
struct CacheWorkKeyHash
{
   size_t operator()(const CacheWorkKey& key) const
   {
      size_t seed = 0;

      boost::hash_combine(seed, boost::hash_value(key.sourcePath) );
      boost::hash_combine(seed, boost::hash_value(key.destPath) );
      boost::hash_combine(seed, boost::hash_value(key.type) );

      return seed;
   }
};

/**
 * A hash map for CacheWorkKeys (source path, CacheWorkType) to CacheWork pointers.
 */
typedef std::unordered_map<CacheWorkKey, CacheWork*, CacheWorkKeyHash> CacheWorkHashMap;
typedef CacheWorkHashMap::iterator CacheWorkHashMapIter;
typedef CacheWorkHashMap::const_iterator CacheWorkHashMapConstIter;
typedef CacheWorkHashMap::value_type CacheWorkHashMapVal;

/**
 * Super class for the CacheWork.
 */
class CacheWork : public Work
{
   public:
      /**
       * constructor
       */
      CacheWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type, CacheWork* newRootWork, CacheWork* newSplitRootWork,
         int newNumFollowedSymlinks, bool rootWorkIsLocked=false)
      {
         sourcePath = newSourcePath;
         destPath = newDestPath;
         deeperFlags = newDeeperFlags;
         workType = type;

         numFollowedSymlinks = newNumFollowedSymlinks;

         childCount = 0;
         if(newRootWork)
         {
            rootWork = newRootWork;
            increaseChildCount(rootWorkIsLocked);
         }
         else
         {
            rootWork = NULL;
         }

         splitCount = 0;
         if(newSplitRootWork)
         {
            splitRootWork = newSplitRootWork;
            increaseSplitCount();
         }
         else
         {
            splitRootWork = NULL;
         }

         doDiscardForRoot = CacheWork_DISCARD_NO;
         doDiscardForSplit = CacheWork_DISCARD_NO;

         doMemDeleteRoot = false;
         doMemDeleteSplit = false;

         state = CacheWorkState_INIT;

#ifdef BEEGFS_DEBUG
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            "Construct work: " + sourcePath +
            "; is root work: " + StringTk::intToStr(rootWork ? false : true) +
            "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif
      }

      /**
       * Destructor.
       */
      ~CacheWork()
      {
#ifdef BEEGFS_DEBUG
         Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
            "Destruct work: " + sourcePath +
            "; is root work: " + StringTk::intToStr(rootWork ? false : true) +
            "; is split work: " + StringTk::intToStr(splitRootWork ? true : false) );
#endif
         // if it is the last child work, delete the root work
         if(getDoMemDeleteRoot() )
            SAFE_DELETE(rootWork);

         // if it is the last split work, delete the split work
         if(getDoMemDeleteSplit() )
            SAFE_DELETE(splitRootWork);
      }

      void stopWork();
      bool shouldStop();

      uint64_t getChildCount();
      CacheWorkState getState();
      bool updateResult(int error);
      void waitUntilChildsFinished();

      void addSplitWork();
      bool changeToSplitWork(CacheWork* newSplitRootWork);

      static std::string cacheWorkStateToString(CacheWorkState state);
      static std::string cacheWorkTypeToString(CacheWorkType type);

      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

      /**
       * Removes the work from the is in processing map and if a error happened work is added to map
       * with work with an error result.
       *
       * Note: Must be called at the end of the process function. This is done by the
       *       CacheWork::process function which also calls the virtual function doProcess which is
       *       a hook function which must be implemented by every derived class.
       */
      virtual void finishWork() = 0;


   protected:
      std::string sourcePath;    // source path to file or directory to process
      std::string destPath;      // destination path to file or directory to process
      int deeperFlags;           // the deeper flags DEEPER_PREFETCH_... or DEEPER_FLUSH_...
      CacheWorkType workType;    // the type of the work package prefetch or flush

      int numFollowedSymlinks;   // the number of followed symlinks

      AtomicInt16 result;        // the result of the work package, if failed the errno

      CacheWorkState state;      // the state of the work CacheWorkState_...
      Mutex lockState;           // a lock for the state and the childcount

      uint64_t childCount;       // counter for children of the root work
      CacheWork* rootWork;       // pointer to initial work package or NULL if it is the root work

      uint64_t splitCount;       // counter of splits of this work
      CacheWork* splitRootWork;  // pointer to initial work package of the split or NULL

      int doDiscardForRoot;      // 0 no discard, 1 discard including subdirs, 2 discard only dir
      int doDiscardForSplit;     // 0 no discard, 1 discard including subdirs, 2 discard only dir

      bool doMemDeleteRoot;
      bool doMemDeleteSplit;


      /**
       * The hook function which must be implemented by every derived class, this function is called
       * by the function CacheWork::process.
       *
       * @param bufIn Input buffer.
       * @param bufInLen Length of the input buffer.
       * @param bufOut Output buffer.
       * @param bufOutLen Length of the output buffer.
       */
      virtual void doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen) = 0;

      /**
       * Must be called at the beginning of the process function, This is done by the
       * CacheWork::process function which also calls the virtual function doProcess which is a hook
       * function which must be implemented by every derived class.
       *
       * @return True if the processing can start, else false (stop function was called)
       */
      virtual bool startWork() = 0;

      uint64_t increaseChildCount(bool rootWorkIsLocked=false);
      uint64_t increaseChildCountUnlocked();
      uint64_t decreaseChildCount();
      uint64_t decreaseChildCountUnlocked();

      uint64_t increaseSplitCount();
      uint64_t increaseSplitCountUnlocked();
      uint64_t decreaseSplitCount();
      uint64_t decreaseSplitCountUnlocked();

      void setStateToFinished();
      void setStateToFinishedUnlocked();


   public:
      /**
       * Generates a key for the hash map.
       *
       * @param outKey The update key for the hash map.
       */
      void getKey(CacheWorkKey& outKey)
      {
         outKey.sourcePath = sourcePath;
         outKey.destPath = destPath;
         outKey.type = workType;
      }

      /**
       * Getter for the source path which should be processed by the work.
       *
       * @return The source path.
       */
      std::string* getSourcePath()
      {
         return &sourcePath;
      }

      /**
       * Getter for the destination path which should be processed by the work.
       *
       * @return The destination path.
       */
      std::string* getDestinationPath()
      {
         return &destPath;
      }

      /**
       * Getter for the deeper flags DEEPER_PREFETCH_... or DEEPER_FLUSH_....
       *
       * @return The deeper flags.
       */
      int getDeeperFlags()
      {
         return deeperFlags;
      }

      /**
       * Getter for the type of the work.
       *
       * @return The work type, a value of the enum CacheWorkType.
       */
      CacheWorkType getWorkType()
      {
         return workType;
      }

      /**
       * Getter for the result of this work.
       *
       * @return DEEPER_RETVAL_SUCCESS on success, or the errno on error
       */
      int getResult()
      {
         return result.read();
      }

      /**
       * Prints all required informations for a log message.
       */
      std::string printForLog()
      {
         return "sourcePath: " + sourcePath +
            "; destPath: " + destPath +
            "; deeperFlags: " + StringTk::intToStr(deeperFlags) +
            "; workType: " + cacheWorkTypeToString(workType) +
            "; isRootWork: " + StringTk::intToStr(rootWork ? false : true);
      }

      /**
       * Checks if a the work is finished.
       *
       * @return True if the work is finished.
       */
      bool isFinished()
      {
         return state == CacheWorkState_FINISHED;
      }

      /**
       * Checks if this work is a split work.
       *
       * @return True if the work is a split work.
       */
      bool isSplitWork()
      {
         if(splitRootWork)
         {
            return true;
         }
         else
         {
            return false;
         }
      }

      /**
       * Enables discard for this work. The discard will be executed by the work itself or by the
       * last child work.
       */
      void setDoDiscardForRoot()
      {
         doDiscardForRoot = CacheWork_DISCARD_SUBDIRS;
      }

      /**
       * Enables discard for this work. The discard will be executed by the work itself or by the
       * last split work.
       */
      void setDoDiscardForSplit()
      {
         doDiscardForSplit = CacheWork_DISCARD_ROOTDIR;
      }

      /**
       * Check if this work should cleanup the root work.
       *
       * @return True if this work should destruct the root work.
       */
      bool getDoMemDeleteRoot()
      {
         bool retVal;

         SafeMutexLock safeLock(&lockState);
         retVal = doMemDeleteRoot;
         safeLock.unlock();

         return retVal;
      }

      /**
       * Check if this work should cleanup the split root work.
       *
       * @return True if this work should destruct the split root work.
       */
      bool getDoMemDeleteSplit()
      {
         bool retVal;

         SafeMutexLock safeLock(&lockState);
         retVal = doMemDeleteSplit;
         safeLock.unlock();

         return retVal;
      }

      /**
       * Enables the memory delete for the root work by this work.
       *
       * NOTE: It doesn't lock the state lock. The state lock must be locked before.
       */
      void setDoMemDeleteRootUnlocked()
      {
         doMemDeleteRoot = true;
      }

      /**
       * Enables the memory delete for the root work by this work.
       *
       * NOTE: It doesn't lock the state lock. The state lock must be locked before.
       */
      void setDoMemDeleteSplitUnlocked()
      {
         doMemDeleteSplit = true;
      }
};

#endif /* COMPONENTS_WORKER_CACHEWORK_H_ */
