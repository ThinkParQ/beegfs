#ifndef GETHIGHRESSTATSRESPMSG_H_
#define GETHIGHRESSTATSRESPMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/Common.h>


class GetHighResStatsRespMsg : public NetMessage
{
   public:

      /**
       * @param statsList just a reference, so do not free it as long as you use this object!
       */
      GetHighResStatsRespMsg(HighResStatsList* statsList) :
         NetMessage(NETMSGTYPE_GetHighResStatsResp)
      {
         this->statsList = statsList;
      }

      GetHighResStatsRespMsg() : NetMessage(NETMSGTYPE_GetHighResStatsResp)
      {
      }


   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenHighResStatsList(statsList);
      }


   private:
      // for serialization
      HighResStatsList* statsList; // not owned by this object!

      // for deserialization
      unsigned statsListElemNum; // NETMSG_NICLISTELEM_SIZE defines the element size
      const char* statsListStart; // see NETMSG_NICLISTELEM_SIZE for element structure


   public:

      // inliners
      void parseStatsList(HighResStatsList* outList)
      {
         Serialization::deserializeHighResStatsList(statsListElemNum, statsListStart, outList);
      }
};

#endif /* GETHIGHRESSTATSRESPMSG_H_ */
