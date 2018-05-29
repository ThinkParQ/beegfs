#ifndef COMPONENTS_WORKER_COPYFILECRCWORK_H_
#define COMPONENTS_WORKER_COPYFILECRCWORK_H_


#include <components/worker/DeeperWork.h>



class CopyFileCrcWork : public DeeperWork
{
   public:
      CopyFileCrcWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, NULL, NULL, 0) {};
      CopyFileCrcWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type, CacheWork* newRootWork) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, newRootWork, NULL, 0) {};
      ~CopyFileCrcWork() {};

      void doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

      /**
       * Prints all required informations for a log message.
       */
      std::string printForLog()
      {
         return "CopyFileCrcWork: " + CacheWork::printForLog();
      }
};

#endif /* COMPONENTS_WORKER_COPYFILECRCWORK_H_ */
