#ifndef STORAGEBENCHCONTROLMSGRESP_H_
#define STORAGEBENCHCONTROLMSGRESP_H_

#include <common/benchmark/StorageBench.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>


class StorageBenchControlMsgResp: public NetMessage
{
   public:
      /*
       * @param errorCode STORAGEBENCH_ERROR_...
       */
      StorageBenchControlMsgResp(StorageBenchStatus status, StorageBenchAction action,
         StorageBenchType type, int errorCode, StorageBenchResultsMap& results) :
         NetMessage(NETMSGTYPE_StorageBenchControlMsgResp)
      {
         this->status = status;
         this->action = action;
         this->type = type;
         this->errorCode = errorCode;

         for (StorageBenchResultsMapIter iter = results.begin(); iter != results.end(); iter++)
         {
            this->resultTargetIDs.push_back(iter->first);
            this->resultValues.push_back(iter->second);
         }
      }

      /**
       * Constructor for deserialization only
       */
      StorageBenchControlMsgResp() : NetMessage(NETMSGTYPE_StorageBenchControlMsgResp)
      {
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // status
            Serialization::serialLenInt() + // action
            Serialization::serialLenInt() + // type
            Serialization::serialLenInt() + // errorCode
            Serialization::serialLenUInt16List(&this->resultTargetIDs) + // resultTargetIDs
            Serialization::serialLenInt64List(&this->resultValues); // resultValues
      }

   private:
      int status;                // StorageBenchStatus
      int action;                // StorageBenchAction
      int type;                  // StorageBenchType
      int errorCode;             // STORAGEBENCH_ERROR...
      UInt16List resultTargetIDs;
      Int64List resultValues;

      // deserialization info
     unsigned resultTargetIDsElemNum;
     const char* resultTargetIDsListStart;
     unsigned resultTargetIDsBufLen;

     unsigned resultValuesElemNum;
     const char* resultValuesListStart;
     unsigned resultValuesBufLen;

   public:
      //inliners

      StorageBenchStatus getStatus()
      {
         return (StorageBenchStatus)this->status;
      }

      StorageBenchAction getAction()
      {
         return (StorageBenchAction)this->action;
      }

      StorageBenchType getType()
      {
         return (StorageBenchType)this->type;
      }

      int getErrorCode()
      {
         return this->errorCode;
      }

      void parseResults(StorageBenchResultsMap* outResults)
      {
         UInt16List resultTargetIds;
         Serialization::deserializeUInt16List(this->resultTargetIDsBufLen,
            this->resultTargetIDsElemNum, this->resultTargetIDsListStart, &resultTargetIds);

         Int64List resultValues;
         Serialization::deserializeInt64List(this->resultValuesBufLen, this->resultValuesElemNum,
            this->resultValuesListStart, &resultValues);

         Int64ListIter iterValues = resultValues.begin();
         for (UInt16ListIter iterTarget = resultTargetIds.begin();
            iterTarget != resultTargetIds.end(); iterTarget++, iterValues++)
         {
            (*outResults)[(*iterTarget)] = (*iterValues);
         }
      }
};

#endif /* STORAGEBENCHCONTROLMSGRESP_H_ */
