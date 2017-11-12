#ifndef TIME_H_
#define TIME_H_

#include <common/Common.h>
#include <time.h>

/**
 * This time class is based on a monotonic clock.
 */

class Time
{
   public:
      Time()
      {
         clock_gettime(CLOCK_MONOTONIC, &this->now);
      }

      Time(struct timespec* t)
      {
         this->now = *t;
      }

      Time(const Time& t)
      {
         this->now = t.now;
      }


   private:
      struct timespec now;


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

      void setToNow()
      {
         clock_gettime(CLOCK_MONOTONIC, &this->now);
      }

      /**
       * @return elapsed millisecs
       */
      unsigned elapsedSinceMS(Time* earlierT)
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
      unsigned elapsedSinceMicro(Time* earlierT)
      {
         unsigned secs = (now.tv_sec - earlierT->now.tv_sec) * 1000000;
         int micros = (now.tv_nsec - earlierT->now.tv_nsec) / 1000; /* can also be negative,
                                                                       so this must be signed */

         return secs + micros;
      }

      /**
       * @return elapsed millisecs
       */
      unsigned elapsedMS()
      {
         return Time().elapsedSinceMS(this);
      }

};

#endif /*TIME_H_*/
