#ifndef FULLREFRESHERSLAVE_H_
#define FULLREFRESHERSLAVE_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>

/**
 * This component runs over all metadata files and refreshes the contained information by
 * re-calculating it or re-fetching it from the storage servers. This is typically required to
 * update all metadata after an upgrade.
 *
 * This component is not auto-started when the app starts. It is started and stopped by the
 * FullRefresher.
 */
class FullRefresherSlave : public PThread
{
   friend class FullRefresher; // (to grant access to internal mutex)

   public:
      FullRefresherSlave() throw(ComponentInitException);
      virtual ~FullRefresherSlave();


   private:
      LogContext log;

      Mutex statusMutex; // protects numRefreshed + isRunning
      Condition isRunningChangeCond;

      uint64_t numDirsRefreshed; // number of refreshed directories so far (discrete)
      uint64_t numFilesRefreshed;  // number of refreshed directories so far (discrete)
      bool isRunning; // true if an instance of this component is currently running


      void updateAllMetadata();
      void updateNotInlinedEntryByID(std::string entryID, uint64_t* inoutNumDirsRefreshed,
         uint64_t* inoutNumFilesRefreshed);


      virtual void run();


   public:
      // getters & setters
      bool getIsRunning(bool isRunning)
      {
         SafeMutexLock safeLock(&statusMutex);

         bool retVal = this->isRunning;

         safeLock.unlock();

         return retVal;
      }

      void getNumRefreshed(uint64_t* outNumDirsRefreshed, uint64_t* outNumFilesRefreshed)
      {
         SafeMutexLock safeLock(&statusMutex);

         *outNumDirsRefreshed = this->numDirsRefreshed;
         *outNumFilesRefreshed = this->numFilesRefreshed;

         safeLock.unlock();
      }


   private:
      // getters & setters

      void setIsRunning(bool isRunning)
      {
         SafeMutexLock safeLock(&statusMutex);

         this->isRunning = isRunning;
         isRunningChangeCond.broadcast();

         safeLock.unlock();
      }

      void setNumRefreshed(uint64_t numDirsRefreshed, uint64_t numFilesRefreshed)
      {
         SafeMutexLock safeLock(&statusMutex);

         this->numDirsRefreshed = numDirsRefreshed;
         this->numFilesRefreshed = numFilesRefreshed;

         safeLock.unlock();
      }


};


#endif /* FULLREFRESHERSLAVE_H_ */
