#ifndef NODEOFFLINEWAIT_H_
#define NODEOFFLINEWAIT_H_

#include <common/threading/SafeRWLock.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/OfflineWaitTimeoutTk.h>
#include <app/config/Config.h>

/**
 * Handles timeout during which the node may not send a state report to the mgmtd, so that the mgmtd
 * will eventually set the node to (P)OFFLINE - e.g. when a primary needs a resync right after meta
 * server startup
 */
class NodeOfflineWait
{
   public:
      NodeOfflineWait(Config* cfg)
         : waitTimeoutMS(OfflineWaitTimeoutTk<Config>::calculate(cfg) ),
           active(false)
      { }


   private:
      RWLock rwlock;
      const unsigned waitTimeoutMS;

      Time timer;
      bool active;

   public:

      /**
       * Starts the timer.
       */
      void startTimer()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         active = true;
         timer.setToNow();

         safeLock.unlock(); // U N L O C K
      }

      /**
       * Checks if the timer is running and has not run out yet.
       */
      bool hasTimeout()
      {
         bool res = true;
         {
            SafeRWLock lock(&rwlock, SafeRWLock_READ); // L O C K
            res = active;
            lock.unlock(); // U N L O C K
         }

         if (res) // If active flag is set, timer was still running the last time we checked.
         {        // Check it again and clear active flag if it has run out in the mean time.
            SafeRWLock lock(&rwlock, SafeRWLock_WRITE); // L O C K

            // else: Active flag is set - check if timer has run out yet.
            unsigned elapsedMS = timer.elapsedMS();
            if (elapsedMS >= waitTimeoutMS)
               res = active = false;

            lock.unlock(); // U N L O C K

            if (res)
               LogContext("Node Timeout").log(Log_WARNING,
                  "This node was a primary node of a mirror group and needs a resync. "
                  "Waiting until it is marked offline on all clients. (" +
                  StringTk::uintToStr( (this->waitTimeoutMS - elapsedMS) / 1000) +
                  " seconds left)");
         }

         return res;
      }
};

#endif /* NODEOFFLINEWAIT_H_ */
