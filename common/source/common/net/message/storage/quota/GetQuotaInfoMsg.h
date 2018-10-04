#ifndef GETQUOTAINFOMSG_H_
#define GETQUOTAINFOMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/StoragePoolStore.h>
#include <common/storage/quota/QuotaData.h>
#include <common/storage/quota/GetQuotaConfig.h>

#define GETQUOTAINFOMSG_FEATURE_QUOTA_PER_TARGET         1


class GetQuotaInfoMsg: public NetMessageSerdes<GetQuotaInfoMsg>
{
   friend class TestSerialization;

   public:
      enum QuotaQueryType
      {
         QUERY_TYPE_NONE = 0,
         QUERY_TYPE_SINGLE_ID = 1,
         QUERY_TYPE_ID_RANGE = 2,
         QUERY_TYPE_ID_LIST = 3,
         QUERY_TYPE_ALL_ID = 4
      };

   public:
      /*
       * @param type user or group quota
       * @param storagePoolId the storage pool to fetch data for; only relevant when sent to mgmtd
       */
      GetQuotaInfoMsg(QuotaDataType type, StoragePoolId storagePoolId) :
         BaseType(NETMSGTYPE_GetQuotaInfo),
         type(type),
         targetSelection(GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST),
         targetNumID(0),
         storagePoolId(storagePoolId)  { }

      /*
       * deserialization only
       */
      GetQuotaInfoMsg() : BaseType(NETMSGTYPE_GetQuotaInfo)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->queryType
            % obj->type;

         if(obj->queryType == QUERY_TYPE_ID_RANGE)
            ctx
               % obj->idRangeStart
               % obj->idRangeEnd;

         if(obj->queryType == QUERY_TYPE_ID_LIST)
            ctx % obj->idList;

         if(obj->queryType == QUERY_TYPE_SINGLE_ID)
            ctx % obj->idRangeStart;

         ctx
            % obj->targetSelection
            % obj->targetNumID
            % obj->storagePoolId;
      }

   private:
      int32_t queryType;          // QuotaQueryType
      int32_t type;               // QuotaDataType
      uint32_t idRangeStart;
      uint32_t idRangeEnd;
      UIntList idList;
      uint32_t targetSelection;
      uint16_t targetNumID;
      StoragePoolId storagePoolId;

   public:
      //getter and setter
      unsigned getIDRangeStart() const
      {
         return this->idRangeStart;
      }

      unsigned getIDRangeEnd() const
      {
         return this->idRangeEnd;
      }

      QuotaDataType getType() const
      {
         return (QuotaDataType)type;
      }

      QuotaQueryType getQueryType() const
      {
         return (QuotaQueryType)queryType;
      }

      unsigned getID() const
      {
         return this->idRangeStart;
      }

      UIntList* getIDList()
      {
         return &this->idList;
      }

      void setQueryType(QuotaQueryType queryType)
      {
         this->queryType = queryType;
      }

      void setID(unsigned id)
      {
         this->queryType = QUERY_TYPE_SINGLE_ID;
         this->idRangeStart = id;
      }

      void setIDRange(unsigned idRangeStart, unsigned idRangeEnd)
      {
         this->queryType = QUERY_TYPE_ID_RANGE;
         this->idRangeStart = idRangeStart;
         this->idRangeEnd = idRangeEnd;
      }

      uint16_t getTargetNumID() const
      {
         return this->targetNumID;
      }

      unsigned getTargetSelection() const
      {
         return this->targetSelection;
      }

      StoragePoolId getStoragePoolId() const
      {
         return storagePoolId;
      }

      void setTargetSelectionSingleTarget(uint16_t newTargetNumID)
      {
         this->targetNumID = newTargetNumID;
         this->targetSelection = GETQUOTACONFIG_SINGLE_TARGET;
      }

      void setTargetSelectionAllTargetsOneRequestPerTarget(uint16_t newTargetNumID)
      {
         this->targetNumID = newTargetNumID;
         this->targetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET;
      }

      void setTargetSelectionAllTargetsOneRequestForAllTargets()
      {
         this->targetNumID = 0;
         this->targetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST;
      }
};

#endif /* GETQUOTAINFOMSG_H_ */
