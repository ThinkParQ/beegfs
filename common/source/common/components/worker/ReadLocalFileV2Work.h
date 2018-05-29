#ifndef READLOCALFILEV2WORK_H_
#define READLOCALFILEV2WORK_H_

#include <common/Common.h>
#include <common/net/sock/Socket.h>
#include <common/components/worker/Work.h>
#include <common/storage/PathInfo.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/nodes/NumNodeID.h>
#include <common/nodes/TargetMapper.h>
#include <common/nodes/NodeStoreServers.h>


struct ReadLocalFileWorkInfo
{
   ReadLocalFileWorkInfo(NumNodeID localNodeNumID, TargetMapper* targetMapper,
      NodeStoreServers* storageNodes, SynchronizedCounter* counter) :
      localNodeNumID(localNodeNumID), targetMapper(targetMapper), storageNodes(storageNodes),
      counter(counter)
   {
      // all init done in initializer list
   }

   NumNodeID localNodeNumID;
   TargetMapper* targetMapper;
   NodeStoreServers* storageNodes;
   SynchronizedCounter* counter;
};


class ReadLocalFileV2Work : public Work
{
   public:
      ReadLocalFileV2Work(const char* fileHandleID, char* buf, unsigned accessFlags, off_t offset,
         size_t size, uint16_t targetID, PathInfo* pathInfoPtr, int64_t* nodeResult,
         ReadLocalFileWorkInfo* readInfo, bool firstWriteDoneForTarget)
      {
         this->fileHandleID   = fileHandleID;
         this->buf            = buf;
         this->accessFlags    = accessFlags;
         this->offset         = offset;
         this->size           = size;
         this->targetID       = targetID;
         this->pathInfoPtr    = pathInfoPtr;
         this->nodeResult     = nodeResult;
         this->readInfo       = readInfo;
         this->firstWriteDoneForTarget = firstWriteDoneForTarget;
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
      PathInfo* pathInfoPtr;
      int64_t* nodeResult;
      ReadLocalFileWorkInfo* readInfo;

      bool firstWriteDoneForTarget; /* true if the first chunk was written to the storage target,
                                       it's needed for the session check*/

      int64_t communicate(Node& node, char* bufIn, unsigned bufInLen,
         char* bufOut, unsigned bufOutLen);

};

#endif /*READLOCALFILEV2WORK_H_*/
