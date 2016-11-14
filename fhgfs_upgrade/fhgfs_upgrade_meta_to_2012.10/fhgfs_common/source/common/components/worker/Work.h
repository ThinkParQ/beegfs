#ifndef WORK_H_
#define WORK_H_

#include <common/toolkit/HighResolutionStats.h>
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
      
      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen) = 0;
      
      
   private:
      
      
   protected:
      HighResolutionStats stats;
      
      
   public:
      // getters & setters
      
      HighResolutionStats* getHighResolutionStats()
      {
         return &stats;
      }
};

#endif /*WORK_H_*/
