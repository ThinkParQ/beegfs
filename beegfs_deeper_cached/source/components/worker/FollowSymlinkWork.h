#ifndef COMPONENTS_WORKER_FOLLOWSYMLINKWORK_H_
#define COMPONENTS_WORKER_FOLLOWSYMLINKWORK_H_


#include <components/worker/DeeperWork.h>



class FollowSymlinkWork: public DeeperWork
{
   public:
      FollowSymlinkWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, NULL, NULL, 0) {};
      FollowSymlinkWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type, CacheWork* rootWork, int followSymlinkCounter) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, rootWork, NULL,
            followSymlinkCounter) {};
      ~FollowSymlinkWork() {};

      void doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

      /**
       * Prints all required informations for a log message.
       */
      std::string printForLog()
      {
         return "FollowSymlinkWork: " + CacheWork::printForLog();
      }
};

#endif /* COMPONENTS_WORKER_FOLLOWSYMLINKWORK_H_ */
