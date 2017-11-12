#ifndef READLOCALFILEV2WORK_H_
#define READLOCALFILEV2WORK_H_

#include <common/Common.h>
#include <common/net/sock/Socket.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/SynchronizedCounter.h>


struct ReadLocalFileWorkInfo
{
   ReadLocalFileWorkInfo(const char* localNodeID, TargetMapper* targetMapper,
      NodeStoreServers* storageNodes, SynchronizedCounter* counter) :
      localNodeID(localNodeID), targetMapper(targetMapper), storageNodes(storageNodes),
      counter(counter)
   {
      // all init done in initializer list
   }

   const char* localNodeID;
   TargetMapper* targetMapper;
   NodeStoreServers* storageNodes;
   SynchronizedCounter* counter;
};


class ReadLocalFileV2Work : public Work
{
   public:
      ReadLocalFileV2Work(const char* fileHandleID, char* buf, unsigned accessFlags, off_t offset,
         size_t size, uint16_t targetID, int64_t* nodeResult, ReadLocalFileWorkInfo* readInfo)
      {
         this->fileHandleID = fileHandleID;
         this->buf = buf;
         this->accessFlags = accessFlags;
         this->offset = offset;
         this->size = size;
         this->targetID = targetID;
         this->nodeResult = nodeResult;
         this->readInfo = readInfo;
      }

      virtual ~ReadLocalFileV2Work()
      {
      }

      
      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);
      
   private:
      const char* fileHandleID;
      char* buf;
      unsigned accessFlags;
      off_t offset;
      size_t size;
      uint16_t targetID;
      int64_t* nodeResult;
      ReadLocalFileWorkInfo* readInfo;
      
      int64_t communicate(Node* node, char* bufIn, unsigned bufInLen,
         char* bufOut, unsigned bufOutLen);
      
};

#endif /*READLOCALFILEV2WORK_H_*/
