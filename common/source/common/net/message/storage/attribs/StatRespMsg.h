#ifndef STATRESPMSG_H_
#define STATRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/Metadata.h>
#include <common/storage/StatData.h>
#include <common/Common.h>


#define STATRESPMSG_FLAG_HAS_PARENTINFO                  1 /* msg includes parentOwnerNodeID and
                                                              parentEntryID */


class StatRespMsg : public NetMessageSerdes<StatRespMsg>
{
   public:
      StatRespMsg(int result, StatData statData) :
         BaseType(NETMSGTYPE_StatResp)
      {
         this->result = result;
         this->statData = statData;
      }

      /**
       * For deserialization only.
       */
      StatRespMsg() : BaseType(NETMSGTYPE_StatResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % obj->statData.serializeAs(StatDataFormat_NET);

         if(obj->isMsgHeaderFeatureFlagSet(STATRESPMSG_FLAG_HAS_PARENTINFO) )
         {
            ctx
               % serdes::stringAlign4(obj->parentEntryID)
               % obj->parentNodeID;
         }
      }

      virtual unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return STATRESPMSG_FLAG_HAS_PARENTINFO;
      }

   private:
      int32_t result;
      StatData statData;

      NumNodeID parentNodeID;
      std::string parentEntryID;

   public:
      // getters & setters
      int getResult()
      {
         return result;
      }

      StatData* getStatData()
      {
         return &this->statData;
      }

      void addParentInfo(NumNodeID parentNodeID, std::string parentEntryID)
      {
         this->parentNodeID  = parentNodeID;
         this->parentEntryID = parentEntryID;

         addMsgHeaderFeatureFlag(STATRESPMSG_FLAG_HAS_PARENTINFO);
      }

      NumNodeID getParentNodeID()
      {
         return this->parentNodeID;
      }
};

#endif /*STATRESPMSG_H_*/
