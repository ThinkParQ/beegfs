#ifndef TIMEABS_H_
#define TIMEABS_H_

#include <common/Common.h>
#include <sys/time.h>

/**
 * This time class is based on a non-monotonic clock (based on the epoch since 1970) and thus
 * might jump forwards or backwards if the admin or a tool updates the system time.
 */
class TimeAbs
{
   public:
      TimeAbs()
      {
         gettimeofday(&this->now, NULL);
      }

      TimeAbs(struct timeval* t)
      {
         this->now = *t;
      }

      TimeAbs(const TimeAbs& t)
      {
         this->now = t.now;
      }


   private:
      struct timeval now;


   public:
      // inliners
      bool operator == (const TimeAbs& t) const
      {
         return (now.tv_usec == t.now.tv_usec) && (now.tv_sec == t.now.tv_sec);
      }

      bool operator != (const TimeAbs& t) const
      {
         return (now.tv_usec != t.now.tv_usec) || (now.tv_sec != t.now.tv_sec);
      }

      TimeAbs& operator = (const TimeAbs& t)
      {
          if(this != &t)
             this->now = t.now;

          return *this;
      }

      void setToNow()
      {
         gettimeofday(&now, NULL);
      }

      /**
       * @return elapsed millisecs
       */
      unsigned elapsedSinceMS(const TimeAbs* earlierT) const
      {
         unsigned secs = (now.tv_sec - earlierT->now.tv_sec) * 1000;
         int micros = (now.tv_usec - earlierT->now.tv_usec) / 1000; /* can also be negative,
                                                                       so this must be signed */

         return secs + micros;
      }

      /**
       * Note: Be careful with this to avoid integer overflows
       *
       * @return elapsed microsecs
       */
      unsigned elapsedSinceMicro(const TimeAbs* earlierT) const
      {
         unsigned secs = (now.tv_sec - earlierT->now.tv_sec) * 1000000;
         int micros = (now.tv_usec - earlierT->now.tv_usec); /* can also be negative,
                                                                so this must be signed */

         return secs + micros;
      }

      /**
       * @return elapsed millisecs
       */
      unsigned elapsedMS() const
      {
         return TimeAbs().elapsedSinceMS(this);
      }

      // getters & setters
      struct timeval* getTimeval()
      {
         return &now;
      }

      /**
       * @return seconds since the epoch
       */
      uint64_t getTimeS()
      {
         return now.tv_sec;
      }

      /**
       * @return milliseconds since the epoch
       */
      uint64_t getTimeMS()
      {
         return (now.tv_sec * 1000LL) + (now.tv_usec / 1000LL);
      }

      /**
       * get microseconds part of the current second
       */
      uint64_t getTimeMicroSecPart()
      {
         return now.tv_usec;
      }

};

#endif /* TIMEABS_H_ */
