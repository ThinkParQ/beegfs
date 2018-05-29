#ifndef COMPONENTS_WORKER_DEEPERWORK_H_
#define COMPONENTS_WORKER_DEEPERWORK_H_


#include <common/cache/components/worker/CacheWork.h>



class DeeperWork : public CacheWork
{
   public:
      /**
       * Constructor.
       */
      DeeperWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type, CacheWork* newRootWork, CacheWork* newSplitWork,
         int followSymlinkCounter, bool rootWorkIsLocked=false) :
            CacheWork(newSourcePath, newDestPath, newDeeperFlags, type, newRootWork, newSplitWork,
            followSymlinkCounter, rootWorkIsLocked) {};

      virtual void doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen) = 0;

   protected:
      void childFinished(CacheWork* child);
      void splitFinished(CacheWork* split);

   private:
      virtual bool startWork();
      virtual void finishWork();
};

#endif /* COMPONENTS_WORKER_DEEPERWORK_H_ */
