#ifndef SETEXCEEDEDQUOTAWORK_H_
#define SETEXCEEDEDQUOTAWORK_H_


#include <common/Common.h>
#include <common/components/worker/Work.h>
#include <common/net/message/storage/quota/SetExceededQuotaMsg.h>
#include <common/nodes/Node.h>
#include <common/storage/quota/QuotaData.h>
#include <common/toolkit/SynchronizedCounter.h>


class SetExceededQuotaWork : public Work
{
   public:
      SetExceededQuotaWork(StoragePoolId storagePoolId, QuotaDataType idType,
         QuotaLimitType limitType, Node& storageNode, int messageNumber, UIntList* idList,
         SynchronizedCounter *counter, int* result) :
         storageNode(storageNode)
      {
         this->storagePoolId = storagePoolId;
         this->messageNumber = messageNumber;

         this->idType = idType;
         this->limitType = limitType;
         this->idList = idList;

         this->counter = counter;
         this->result = result;
      }

      virtual ~SetExceededQuotaWork()
      {
      }

      virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);


   protected:


   private:
      StoragePoolId storagePoolId;

      Node& storageNode;
      int messageNumber;

      QuotaDataType idType;
      QuotaLimitType limitType;
      UIntList* idList;

      SynchronizedCounter* counter;
      int* result;


      void prepareMessage(int messageNumber, SetExceededQuotaMsg* msg);
};

#endif /* SETEXCEEDEDQUOTAWORK_H_ */
