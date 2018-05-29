#ifndef COMPONENTS_WORKER_COPYFILERANGEWORK_H_
#define COMPONENTS_WORKER_COPYFILERANGEWORK_H_


#include <components/worker/DeeperWork.h>



class CopyFileRangeWork : public DeeperWork
{
   public:
      CopyFileRangeWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type, off_t startPosition, size_t numBytes) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, NULL, NULL, 0),
            startPosition(startPosition), numBytes(numBytes) {};
      CopyFileRangeWork(std::string newSourcePath, std::string newDestPath, int newDeeperFlags,
         CacheWorkType type, CacheWork* rootWork, CacheWork* splitWork,
         int followSymlinkCounter, off_t startPosition, size_t numBytes) :
            DeeperWork(newSourcePath, newDestPath, newDeeperFlags, type, rootWork, splitWork,
            followSymlinkCounter), startPosition(startPosition), numBytes(numBytes) {};
      ~CopyFileRangeWork() {};

      void doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   private:
      off_t startPosition;
      size_t numBytes;


   public:
      /**
       * Prints all required informations for a log message.
       */
      std::string printForLog()
      {
         return "CopyFileRangeWork: " + CacheWork::printForLog() +
            "; startPosition: " + StringTk::int64ToStr(startPosition) +
            "; numBytes: " + StringTk::int64ToStr(numBytes);
      }
};

#endif /* COMPONENTS_WORKER_COPYFILERANGEWORK_H_ */
