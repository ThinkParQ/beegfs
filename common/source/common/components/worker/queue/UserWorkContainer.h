#ifndef USERWORKCONTAINER_H_
#define USERWORKCONTAINER_H_

#include <common/toolkit/NamedException.h>
#include <common/Common.h>
#include "AbstractWorkContainer.h"


DECLARE_NAMEDEXCEPTION(UserWorkContainerException, "UserWorkContainerException")


/**
 * Implementation of AbstractWorkContainer interface based on per-user queues for improved fairness.
 *
 * Work packets will be taken from the different user queues in a round-robin fashion, so that in
 * theory a user with a short queue has the same chance of being assigned to the next free worker as
 * another user with a long queue.
 * Of course, the global fairness is influenced by other scheduling as well, e.g. at the network
 * and disk level.
 *
 * Note on nextWorkIter: A possible unwanted unfairness happens if we have e.g. two active users,
 * where userA writes with n threads, userB writes with only a single thread; then we would see a
 * 2:1 msg processing ratio with a single worker like B, A1, A2, B, A1, A2, B, ...
 * This is because when A1 msg is popped, next iter points to end(), because next B request isn't in
 * queue yet, so on next pop we use begin(), which is A2 even though the next B request came in
 * while we processed A1.
 * We accept this, because avoiding unwanted anomalies completely would get very complex, even with
 * a lastWorkIter (e.g. would require starvation handling when new work is inserted between "last"
 * and "next") or keeping empty queues for another round.
 */
class UserWorkContainer : public AbstractWorkContainer
{
   // typedefs...
   typedef std::map<unsigned, WorkList> UserWorkMap; // key: userID; value: per-user work queue
   typedef UserWorkMap::iterator UserWorkMapIter;
   typedef UserWorkMap::const_iterator UserWorkMapCIter;
   typedef UserWorkMap::value_type UserWorkMapVal;


   public:
      UserWorkContainer()
      {
         numWorks = 0;
         nextWorkIter = workMap.begin(); // initialize by pointing to end() (=> map empty)
      }

      virtual ~UserWorkContainer()
      {
         // walk over all userIDs and delete all elems of their queues...

         for(UserWorkMapIter mapIter = workMap.begin(); mapIter != workMap.end(); mapIter++)
         {
            WorkList& workList = mapIter->second;

            for(WorkListIter listIter = workList.begin(); listIter != workList.end(); listIter++)
               delete(*listIter);
         }
      }

   private:
      UserWorkMap workMap; // entries added on demand and removed when queue empty
      size_t numWorks; // number of works in all queues
      UserWorkMapIter nextWorkIter; // points to next work (or end() if currently not set)


   public:
      // inliners

      Work* getAndPopNextWork()
      {
         #ifdef BEEGFS_DEBUG
            // sanity check
            if(workMap.empty() )
               throw UserWorkContainerException(
                  "Sanity check failed in " + std::string(__func__) + ": "
                  "caller tried to get work from empty queue");
         #endif // BEEGFS_DEBUG

         if(nextWorkIter == workMap.end() )
            nextWorkIter = workMap.begin();

         /* note on iters after map modification: The C++ standard mandates...
            "The insert members shall not affect the validity of iterators and references to the
            container, and the erase members shall invalidate only iterators and references to the
            erased elements." */

         UserWorkMapIter currentWorkIter = nextWorkIter;
         WorkList& workList = currentWorkIter->second;

         nextWorkIter++; // move to next elem


         #ifdef BEEGFS_DEBUG
            // sanity check
            if(workList.empty() )
               throw UserWorkContainerException(
                  "Sanity check failed in " + std::string(__func__) + ": "
                  "user queue is empty");
         #endif // BEEGFS_DEBUG

         Work* work = *workList.begin();
         workList.pop_front();

         numWorks--;

         if(workList.empty() )
            workMap.erase(currentWorkIter); // remove userID with empty work list

         #ifdef BEEGFS_DEBUG
         // sanity checks...

         if(numWorks < workMap.size() )
               throw UserWorkContainerException(
                  "Sanity check failed in " + std::string(__func__) + ": "
                  "numWorks < workMap.size(): " +
                  StringTk::uintToStr(numWorks) + "<" + StringTk::uintToStr(workMap.size() ) );

            if(workMap.empty() && numWorks)
               throw UserWorkContainerException(
                  "Sanity check failed in " + std::string(__func__) + ": "
                  "workMap is empty, but numWorks==" + StringTk::uintToStr(numWorks) );
         #endif // BEEGFS_DEBUG

         return work;
      }

      void addWork(Work* work, unsigned userID)
      {
         // (note: the [] operator implicitly created the key/value pair if it didn't exist yet)
         workMap[userID].push_back(work);

         numWorks++;
      }

      size_t getSize()
      {
         return numWorks;
      }

      bool getIsEmpty()
      {
         return !numWorks;
      }

      void getStatsAsStr(std::string& outStats)
      {
         std::ostringstream statsStream;

         statsStream << "* Queue type: UserWorkContainer" << std::endl;
         statsStream << "* Num works total: " << numWorks << std::endl;
         statsStream << "* Num user queues: " << workMap.size() << std::endl;

         statsStream << std::endl;

         if(workMap.empty() )
         { // no individual stats to be printed
            outStats = statsStream.str();
            return;
         }

         statsStream << "Individual queue stats (user/qlen)..." << std::endl;

         for(UserWorkMapCIter mapIter = workMap.begin(); mapIter != workMap.end(); mapIter++)
         {
            // we use int for userID because NETMSG_DEFAULT_USERID looks better signed
            int userID = mapIter->first;
            const WorkList& workList = mapIter->second;

            statsStream << "* " << "UserID " << userID << ": " << workList.size() << std::endl;
         }

         outStats = statsStream.str();
      }

};


#endif /* USERWORKCONTAINER_H_ */
