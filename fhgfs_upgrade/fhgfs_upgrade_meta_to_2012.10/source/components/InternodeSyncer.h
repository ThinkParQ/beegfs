#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>
#include <common/Common.h>


class InternodeSyncer : public PThread
{
   public:
      InternodeSyncer() throw(ComponentInitException);
      virtual ~InternodeSyncer();


   private:
      LogContext log;

      Mutex forcePoolsUpdateMutex;
      bool forcePoolsUpdate; // true to force update of capacity pools

      virtual void run();
      void syncLoop();

      void updateMetaCapacityPools();
      void updateStorageCapacityPools();
      void updateCapacityPools(NodeType nodeType, TargetCapacityPools* pools);

   public:
      // getters & setters
      void setForcePoolsUpdate()
      {
         SafeMutexLock safeLock(&forcePoolsUpdateMutex);

         this->forcePoolsUpdate = true;

         safeLock.unlock();
      }

      // inliners
      bool getAndResetForcePoolsUpdate()
      {
         SafeMutexLock safeLock(&forcePoolsUpdateMutex);

         bool retVal = this->forcePoolsUpdate;

         this->forcePoolsUpdate = false;

         safeLock.unlock();

         return retVal;
      }
};


#endif /* INTERNODESYNCER_H_ */
