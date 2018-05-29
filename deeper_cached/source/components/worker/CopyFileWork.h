#ifndef COMPONENTS_WORKER_COPYFILEWORK_H_
#define COMPONENTS_WORKER_COPYFILEWORK_H_


#include <components/worker/DeeperWork.h>



class CopyFileWork : public DeeperWork
{
   public:
      CopyFileWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, NULL, NULL, 0) {};
      CopyFileWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type, CacheWork* rootWork, CacheWork* splitWork,
         int followSymlinkCounter) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, rootWork, splitWork,
            followSymlinkCounter) {};
      ~CopyFileWork() {};

      void doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

      /**
       * Prints all required informations for a log message.
       */
      std::string printForLog()
      {
         return "CopyFileWork: " + CacheWork::printForLog();
      }
};

#endif /* COMPONENTS_WORKER_COPYFILEWORK_H_ */
