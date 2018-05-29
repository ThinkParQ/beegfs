#ifndef COMPONENTS_WORKER_DISCARDWORK_H_
#define COMPONENTS_WORKER_DISCARDWORK_H_

#include "DeeperWork.h"

class DiscardWork: public DeeperWork
{
   public:
      DiscardWork(std::string newSourcePath, int newDeeperFlags) :
            DeeperWork(newSourcePath, "", newDeeperFlags, CacheWorkType_FLUSH, NULL, NULL, 0) {};
      DiscardWork(std::string newSourcePath, int newDeeperFlags, CacheWork* rootWork,
            bool rootWorkIsLocked) :  DeeperWork(newSourcePath, "", newDeeperFlags,
            CacheWorkType_FLUSH, rootWork, NULL, 0, rootWorkIsLocked) {};
      ~DiscardWork() {};

      void doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

      /**
       * Prints all required informations for a log message.
       */
      std::string printForLog()
      {
         return "DiscardWork: " + CacheWork::printForLog();
      }
};

#endif /* COMPONENTS_WORKER_DISCARDWORK_H_ */
