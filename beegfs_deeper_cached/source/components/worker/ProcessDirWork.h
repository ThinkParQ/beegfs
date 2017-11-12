#ifndef COMPONENTS_WORKER_PROCESSDIRWORK_H_
#define COMPONENTS_WORKER_PROCESSDIRWORK_H_


#include <components/worker/DeeperWork.h>



class ProcessDirWork : public DeeperWork
{
   public:
      ProcessDirWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, NULL, NULL, 0) {};
      ProcessDirWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type, CacheWork* rootWork, int followSymlinkCounter) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, rootWork, NULL,
            followSymlinkCounter) {};
      ~ProcessDirWork() {};

      void doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

      /**
       * Prints all required informations for a log message.
       */
      std::string printForLog()
      {
         return "ProcessDirWork: " + CacheWork::printForLog();
      }
};

#endif /* COMPONENTS_WORKER_PROCESSDIRWORK_H_ */
