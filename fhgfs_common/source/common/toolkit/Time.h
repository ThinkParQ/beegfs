#ifndef TIME_H_
#define TIME_H_

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>
#include <time.h>

#define TIME_SAFE_CLOCK_ID CLOCK_MONOTONIC // a clock-id we expect to always work

#ifdef CLOCK_MONOTONIC_COARSE
   #define TIME_DEFAULT_CLOCK_ID CLOCK_MONOTONIC_COARSE
#else
   #define TIME_DEFAULT_CLOCK_ID CLOCK_MONOTONIC
#endif


/**
 * This time class is based on a monotonic clock.
 * If the system supports it, this class uses a fast, but coarse-grained clock source, which is only
 * updated every few milliseconds.
 * If you need a more fine-grained clock (with sub-millisecond precision), use class TimeFine.
 */
class Time
{
   public:
      Time()
      {
         clock_gettime(clockID, &this->now);
      }

      /**
       * @param setZero true to set this time to zero, e.g. to mark as uninitialized or
       * "very long ago"; false to skip time init completely (which is only useful for derived
       * classes, which provide their own initialization for "this->now").
       */
      Time(bool setZero)
      {
         if(!setZero)
            return;

         now.tv_sec = 0;
         now.tv_nsec = 0;
      }

      Time(struct timespec* t)
      {
         this->now = *t;
      }

      Time(const Time& t)
      {
         this->now = t.now;
      }

      virtual ~Time() {} // nothing to be done, just need a virtual destructor for derived classes


      static bool testClock();
      static bool testClockID(clockid_t clockID);


   protected:
      struct timespec now;


   private:
      static clockid_t clockID; // clockID for normal time requests


   public:
      // inliners
      bool operator == (const Time& t) const
      {
         return (now.tv_nsec == t.now.tv_nsec) && (now.tv_sec == t.now.tv_sec);
      }

      bool operator != (const Time& t) const
      {
         return (now.tv_nsec != t.now.tv_nsec) || (now.tv_sec != t.now.tv_sec);
      }

      Time& operator = (const Time& t)
      {
          if(this != &t)
             this->now = t.now;

          return *this;
      }

      bool operator < (const Time& t) const
      {
         if (now.tv_sec < t.now.tv_sec)
            return true;

         if ((now.tv_sec == t.now.tv_sec) && (now.tv_nsec < t.now.tv_nsec))
            return true;

         return false;
      }

      bool operator > (const Time& t) const
      {
         return t < *this;
      }

      bool getIsZero() const
      {
         return !now.tv_sec && !now.tv_nsec;
      }

      /**
       * Add the given milli seconds to the current time. Used to calculate timeouts.
       */
      void addMS(const int addTimeMS)
      {
         struct timespec newTime;

         newTime.tv_sec  = now.tv_sec  +  addTimeMS / 1000;
         newTime.tv_nsec = now.tv_nsec + (addTimeMS % 1000) * 1000000;

         // now add tv_nsec > 1000 to tv_sec
         newTime.tv_sec  = newTime.tv_sec + newTime.tv_nsec / (1000*1000*1000);
         newTime.tv_nsec = newTime.tv_nsec % (1000*1000*1000);

         now = newTime;
      }

      virtual void setToNow()
      {
         clock_gettime(clockID, &this->now);
      }

      /**
       * @return elapsed millisecs
       */
      unsigned elapsedSinceMS(const Time* earlierT) const
      {
         unsigned secs = (now.tv_sec - earlierT->now.tv_sec) * 1000;
         int micros = (now.tv_nsec - earlierT->now.tv_nsec) / 1000000; /* can also be negative,
                                                                          so this must be signed */

         return secs + micros;
      }

      /**
       * Note: Be careful with this to avoid integer overflows
       *
       * @return elapsed microsecs
       */
      unsigned elapsedSinceMicro(const Time* earlierT) const
      {
         unsigned secs = (now.tv_sec - earlierT->now.tv_sec) * 1000000;
         int micros = (now.tv_nsec - earlierT->now.tv_nsec) / 1000; /* can also be negative,
                                                                       so this must be signed */

         return secs + micros;
      }

      /**
       * @return elapsed millisecs
       */
      virtual unsigned elapsedMS() const
      {
         return Time().elapsedSinceMS(this);
      }

      virtual unsigned elapsedMicro() const
      {
         return Time().elapsedSinceMicro(this);
      }

      static clockid_t getClockID()
      {
         return clockID;
      }

      void getTimeSpec(struct timespec* outTime)
      {
         *outTime = now;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::as<int64_t>(obj->now.tv_sec)
            % serdes::as<int64_t>(obj->now.tv_nsec);
      }
};

#endif /*TIME_H_*/
