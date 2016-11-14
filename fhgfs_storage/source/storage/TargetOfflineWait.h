#ifndef TARGETOFFLINEWAIT_H_
#define TARGETOFFLINEWAIT_H_

#include <app/config/Config.h>
#include <common/threading/SafeRWLock.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/OfflineWaitTimeoutTk.h>

typedef std::map<uint16_t, Time> TargetTimerMap;
typedef TargetTimerMap::iterator TargetTimerMapIter;

/**
 * Handles timeouts during which targets may not send a state report to the Mgmtd, so that the
 * Mgmtd will eventually set the target to (P)OFFLINE - e.g. when a primary needs a resync right
 * after storage server startup.
 */
class TargetOfflineWait
{
   public:
      TargetOfflineWait(Config* cfg)
         : waitTimeoutMS(OfflineWaitTimeoutTk<Config>::calculate(cfg) )
      { }


   private:
      RWLock rwlock;
      const unsigned waitTimeoutMS;

      /**
       * Maps target IDs to their active timers. If a target has no entry here, it doesn't have an
       * active timeout timer. A target might have an entry here, but the timeout may already be
       * expired. In that case the entry should be removed 'at the earliest convenience', i.e. right
       * after the next check
       */
      TargetTimerMap targetTimerMap;


   public:
      // inliners

      /**
       * Adds a new timer for a target.
       * If there's already a timeout running for that target, it is restarted.
       */
      void addTarget(uint16_t targetID)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         std::pair<TargetTimerMapIter, bool> inserted =
            this->targetTimerMap.insert(std::pair<uint16_t, Time>(targetID, Time() ) );

         if (!inserted.second) // Timer already present, so we have to reset it.
            inserted.first->second.setToNow();

         safeLock.unlock(); // U N L O C K
      }

      /**
       * @returns true if any target timeouts are active at the moment.
       *          Note: An active timeout may already be expired, so it is possible that this
       *          function returns true, but targetHasTimeout returns false for all targets.
       */
      bool anyTargetHasTimeout()
      {
         bool res;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         res = !this->targetTimerMap.empty();

         safeLock.unlock(); // U N L O C K

         return res;
      }

      /**
       * @returns true if the target in question has an active timeout, false if it does not have an
       *          active timeout or the timeout is expired.
       */
      bool targetHasTimeout(uint16_t targetID)
      {
         bool res = false;
         unsigned elapsedMS = 0;

         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         TargetTimerMapIter timer_it = targetTimerMap.find(targetID);

         if (timer_it != targetTimerMap.end() )
         {
            elapsedMS = timer_it->second.elapsedMS();
            if (elapsedMS < this->waitTimeoutMS)
               res = true; // Timeout is not yet expired -> true.
            else
               targetTimerMap.erase(timer_it); // Timeout has already expired -> erase timer.
         }

         safeLock.unlock(); // U N L O C K

         if (res)
            LogContext("Target Timeout").log(Log_WARNING,
               "Target " + StringTk::uintToStr(targetID) + " was a primary target and needs a "
               "resync. Waiting until it is marked offline on all clients. (" +
               StringTk::uintToStr( (this->waitTimeoutMS - elapsedMS) / 1000) + " seconds left)");

         return res;
      }
};

#endif /*TARGETOFFLINEWAIT_H_*/
