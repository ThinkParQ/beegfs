#ifndef STORAGEBENCHCONTROLMSGRESP_H_
#define STORAGEBENCHCONTROLMSGRESP_H_

#include <common/benchmark/StorageBench.h>
#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/ZipIterator.h>


class StorageBenchControlMsgResp: public NetMessageSerdes<StorageBenchControlMsgResp>
{
   public:
      /*
       * @param errorCode STORAGEBENCH_ERROR_...
       */
      StorageBenchControlMsgResp(StorageBenchStatus status, StorageBenchAction action,
         StorageBenchType type, int errorCode, StorageBenchResultsMap& results) :
         BaseType(NETMSGTYPE_StorageBenchControlMsgResp)
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
      StorageBenchControlMsgResp() : BaseType(NETMSGTYPE_StorageBenchControlMsgResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->status
            % obj->action
            % obj->type
            % obj->errorCode
            % obj->resultTargetIDs
            % obj->resultValues;
      }

   private:
      int32_t status;                // StorageBenchStatus
      int32_t action;                // StorageBenchAction
      int32_t type;                  // StorageBenchType
      int32_t errorCode;             // STORAGEBENCH_ERROR...
      UInt16List resultTargetIDs;
      Int64List resultValues;

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
         for (ZipIterRange<Int64List, UInt16List> valuesTargetIDIter(resultValues, resultTargetIDs);
              !valuesTargetIDIter.empty(); ++valuesTargetIDIter)
         {
            (*outResults)[*(valuesTargetIDIter()->second)] = *(valuesTargetIDIter()->first);
         }
      }
};

#endif /* STORAGEBENCHCONTROLMSGRESP_H_ */
