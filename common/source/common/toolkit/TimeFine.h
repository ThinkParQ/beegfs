#ifndef TIMEFINE_H_
#define TIMEFINE_H_

#include <common/toolkit/Time.h>
#include <common/Common.h>


/**
 * This time class is based on a fine-grained monotonic clock with sub-millisecond precision.
 */
class TimeFine : public Time
{
   public:
      TimeFine() : Time(false)
      {
         clock_gettime(CLOCK_MONOTONIC, &this->now);
      }


   private:


   public:
      // inliners

      virtual void setToNow()
      {
         clock_gettime(CLOCK_MONOTONIC, &this->now);
      }

      /**
       * @return elapsed millisecs
       */
      virtual unsigned elapsedMS() const
      {
         return TimeFine().elapsedSinceMS(this);
      }

      virtual unsigned elapsedMicro() const
      {
         return TimeFine().elapsedSinceMicro(this);
      }

};

#endif /* TIMEFINE_H_ */
