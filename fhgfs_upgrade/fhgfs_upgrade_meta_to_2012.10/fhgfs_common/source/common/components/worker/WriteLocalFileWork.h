#ifndef WRITELOCALFILEWORK_H_
#define WRITELOCALFILEWORK_H_

#include <common/Common.h>
#include <common/net/sock/Socket.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/SynchronizedCounter.h>


struct WriteLocalFileWorkInfo
{
   WriteLocalFileWorkInfo(const char* localNodeID, TargetMapper* targetMapper,
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


class WriteLocalFileWork : public Work
{
   public:
      WriteLocalFileWork(const char* fileHandleID, const char* buf, unsigned accessFlags,
         off_t offset, size_t size, uint16_t targetID, int64_t* nodeResult,
         WriteLocalFileWorkInfo* writeInfo)
      {
         this->fileHandleID = fileHandleID;
         this->buf = buf;
         this->accessFlags = accessFlags;
         this->offset = offset;
         this->size = size;
         this->targetID = targetID;
         this->nodeResult = nodeResult;
         this->writeInfo = writeInfo;
      }

      virtual ~WriteLocalFileWork()
      {
      }

      
      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);
      

   private:
      const char* fileHandleID;
      const char* buf;
      unsigned accessFlags;
      off_t offset;
      size_t size;
      uint16_t targetID;
      int64_t* nodeResult;
      WriteLocalFileWorkInfo* writeInfo;
      
      int64_t communicate(Node* node, char* bufIn, unsigned bufInLen,
         char* bufOut, unsigned bufOutLen);
      
};

#endif /*WRITELOCALFILEWORK_H_*/
