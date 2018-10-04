#ifndef MAPTARGETSRESPMSG_H_
#define MAPTARGETSRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>


class MapTargetsRespMsg : public NetMessageSerdes<MapTargetsRespMsg>
{
   public:
      /**
       * @param results indicates the map result of each target sent
       */
      MapTargetsRespMsg(const std::map<uint16_t, FhgfsOpsErr>& results):
            BaseType(NETMSGTYPE_MapTargetsResp), results(&results)
      {
      }

      /**
       * For deserialization only!
       */
      MapTargetsRespMsg():
            BaseType(NETMSGTYPE_MapTargetsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->results, obj->parsed.results);
      }

      const std::map<uint16_t, FhgfsOpsErr>& getResults() const { return *results; }

   private:
      // for serialization
      const std::map<uint16_t, FhgfsOpsErr>* results;

      // for deserialization
      struct
      {
         std::map<uint16_t, FhgfsOpsErr> results;
      } parsed;
};


#endif /* MAPTARGETSRESPMSG_H_ */
