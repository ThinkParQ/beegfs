#ifndef ABSTRACTWORKCONTAINER_H_
#define ABSTRACTWORKCONTAINER_H_

#include <common/components/worker/Work.h>
#include <common/Common.h>


typedef std::list<Work*> WorkList;
typedef WorkList::iterator WorkListIter;


/**
 * Interface for ordered work containers. This is to unify access to trivial std::list based
 * internal work list and the special user-based fair work list.
 */
class AbstractWorkContainer
{
   public:
      virtual ~AbstractWorkContainer() {};

      virtual Work* getAndPopNextWork() = 0;
      virtual void addWork(Work* work, unsigned userID) = 0;

      virtual size_t getSize() = 0;
      virtual bool getIsEmpty() = 0;

      virtual void getStatsAsStr(std::string& outStats) = 0;
};


#endif /* ABSTRACTWORKCONTAINER_H_ */
