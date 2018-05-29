#ifndef WORK_H_
#define WORK_H_

#include <common/toolkit/HighResolutionStats.h>
#include <common/toolkit/TimeFine.h>
#include <common/Common.h>

class Work;

typedef std::list<Work*> WorkList;
typedef WorkList::iterator WorkListIter;

class Work
{
   public:
      Work()
      {
         HighResolutionStatsTk::resetStats(&stats);
      }

      virtual ~Work() {}

      Work(const Work&) = delete;
      Work(Work&&) = delete;
      Work& operator=(const Work&) = delete;
      Work& operator=(Work&&) = delete;

      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen) = 0;

   protected:
      HighResolutionStats stats;

   public:
      HighResolutionStats* getHighResolutionStats()
      {
         return &stats;
      }

#ifdef BEEGFS_DEBUG_PROFILING
      TimeFine* getAgeTime()
      {
         return &age;
      }

   private:
      TimeFine age;
#endif
};

#endif /*WORK_H_*/
