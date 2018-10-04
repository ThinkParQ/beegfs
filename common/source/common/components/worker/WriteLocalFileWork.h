#ifndef WRITELOCALFILEWORK_H_
#define WRITELOCALFILEWORK_H_

#include <common/Common.h>
#include <common/net/sock/Socket.h>
#include <common/components/worker/Work.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/nodes/NumNodeID.h>
#include <common/nodes/TargetMapper.h>
#include <common/nodes/NodeStoreServers.h>


struct WriteLocalFileWorkInfo
{
   WriteLocalFileWorkInfo(NumNodeID localNodeNumID, TargetMapper* targetMapper,
      NodeStoreServers* storageNodes, SynchronizedCounter* counter)
      : localNodeNumID(localNodeNumID), targetMapper(targetMapper), storageNodes(storageNodes),
        counter(counter)
   {
      // all init done in initializer list
   }

   NumNodeID localNodeNumID;
   TargetMapper* targetMapper;
   NodeStoreServers* storageNodes;
   SynchronizedCounter* counter;
};


class WriteLocalFileWork : public Work
{
   public:

      /**
       * Note: pathInfo is not owned by this object.
       */
      WriteLocalFileWork(const char* fileHandleID, const char* buf, unsigned accessFlags,
         off_t offset, size_t size, uint16_t targetID, PathInfo* pathInfo, int64_t* nodeResult,
         WriteLocalFileWorkInfo* writeInfo, bool firstWriteDoneForTarget)
      {
         this->fileHandleID = fileHandleID;
         this->buf = buf;
         this->accessFlags = accessFlags;
         this->offset = offset;
         this->size = size;
         this->targetID = targetID;
         this->pathInfo = pathInfo;
         this->nodeResult = nodeResult;
         this->writeInfo = writeInfo;
         this->firstWriteDoneForTarget = firstWriteDoneForTarget;
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
      PathInfo* pathInfo;
      int64_t* nodeResult;
      WriteLocalFileWorkInfo* writeInfo;
      bool firstWriteDoneForTarget; /* true if the first chunk was written to the storage target,
                                       it's needed for the session check*/

      int64_t communicate(Node& node, char* bufOut, unsigned bufOutLen);

};

#endif /*WRITELOCALFILEWORK_H_*/
