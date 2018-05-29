#ifndef LISTWORKCONTAINER_H_
#define LISTWORKCONTAINER_H_

#include <common/Common.h>
#include "AbstractWorkContainer.h"

/**
 * Simple implementation of AbstractWorkContainer interface based on a single std::list.
 *
 * This container ignores the given userIDs.
 */
class ListWorkContainer : public AbstractWorkContainer
{
   public:
      virtual ~ListWorkContainer()
      {
         for(WorkListIter iter = workList.begin(); iter != workList.end(); iter++)
            delete(*iter);
      }

   private:
      WorkList workList;


   public:
      // inliners

      Work* getAndPopNextWork()
      {
         Work* work = *workList.begin();
         workList.pop_front();

         return work;
      }

      void addWork(Work* work, unsigned userID)
      {
         workList.push_back(work);
      }

      size_t getSize()
      {
         return workList.size();
      }

      bool getIsEmpty()
      {
         return workList.empty();
      }

      void getStatsAsStr(std::string& outStats)
      {
         std::ostringstream statsStream;

         statsStream << "* Queue type: ListWorkContainer" << std::endl;
         statsStream << "* Queue len: " << workList.size() << std::endl;

         outStats = statsStream.str();
      }
};


#endif /* LISTWORKCONTAINER_H_ */
