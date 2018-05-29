#ifndef GETHIGHRESSTATSRESPMSG_H_
#define GETHIGHRESSTATSRESPMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/Common.h>


class GetHighResStatsRespMsg : public NetMessageSerdes<GetHighResStatsRespMsg>
{
   public:

      /**
       * @param statsList just a reference, so do not free it as long as you use this object!
       */
      GetHighResStatsRespMsg(HighResStatsList* statsList) :
         BaseType(NETMSGTYPE_GetHighResStatsResp)
      {
         this->statsList = statsList;
      }

      GetHighResStatsRespMsg() : BaseType(NETMSGTYPE_GetHighResStatsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->statsList, obj->parsed.statsList);
      }

   private:
      // for serialization
      HighResStatsList* statsList; // not owned by this object!

      // for deserialization
      struct {
         HighResStatsList statsList;
      } parsed;


   public:
      HighResStatsList& getStatsList()
      {
         return *statsList;
      }
};

#endif /* GETHIGHRESSTATSRESPMSG_H_ */
