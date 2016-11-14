#ifndef GETQUOTAINFOMSG_H_
#define GETQUOTAINFOMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/quota/QuotaData.h>
#include <common/storage/quota/GetQuotaConfig.h>
#include <common/Common.h>



#define GETQUOTAINFOMSG_FEATURE_QUOTA_PER_TARGET         1

/*
 * enum for the different quota query types
 */
enum QuotaQueryType
{
   GetQuotaInfo_QUERY_TYPE_NONE = 0,
   GetQuotaInfo_QUERY_TYPE_SINGLE_ID = 1,
   GetQuotaInfo_QUERY_TYPE_ID_RANGE = 2,
   GetQuotaInfo_QUERY_TYPE_ID_LIST = 3,
   GetQuotaInfo_QUERY_TYPE_ALL_ID = 4
};


class GetQuotaInfoMsg: public NetMessageSerdes<GetQuotaInfoMsg>
{
   friend class TestSerialization;

   public:
      GetQuotaInfoMsg(QuotaDataType type) : BaseType(NETMSGTYPE_GetQuotaInfo)
      {
         this->type = type;
         this->targetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST;
         this->targetNumID = 0;
      }

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

         if(obj->queryType == GetQuotaInfo_QUERY_TYPE_ID_RANGE)
            ctx
               % obj->idRangeStart
               % obj->idRangeEnd;

         if(obj->queryType == GetQuotaInfo_QUERY_TYPE_ID_LIST)
            ctx % obj->idList;

         if(obj->queryType == GetQuotaInfo_QUERY_TYPE_SINGLE_ID)
            ctx % obj->idRangeStart;

         ctx
            % obj->targetSelection
            % obj->targetNumID;
      }

   private:
      int32_t queryType;          // QuotaQueryType
      int32_t type;               // QuotaDataType
      uint32_t idRangeStart;
      uint32_t idRangeEnd;
      UIntList idList;
      uint32_t targetSelection;
      uint16_t targetNumID;

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
         this->queryType = GetQuotaInfo_QUERY_TYPE_SINGLE_ID;
         this->idRangeStart = id;
      }

      void setIDRange(unsigned idRangeStart, unsigned idRangeEnd)
      {
         this->queryType = GetQuotaInfo_QUERY_TYPE_ID_RANGE;
         this->idRangeStart = idRangeStart;
         this->idRangeEnd = idRangeEnd;
      }

      uint16_t getTargetNumID()
      {
         return this->targetNumID;
      }

      unsigned getTargetSelection() const
      {
         return this->targetSelection;
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
