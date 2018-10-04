#ifndef GETTARGETMAPPINGSRESPMSG_H_
#define GETTARGETMAPPINGSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>

class GetTargetMappingsRespMsg : public NetMessageSerdes<GetTargetMappingsRespMsg>
{
   public:
      /**
       * @param targetIDs just a reference => do not free while you're using this object
       * @param nodeIDs just a reference => do not free while you're using this object
       */
      GetTargetMappingsRespMsg(const std::map<uint16_t, NumNodeID>& mappings) :
         BaseType(NETMSGTYPE_GetTargetMappingsResp),
         mappings(&mappings)
      {
      }

      GetTargetMappingsRespMsg() : BaseType(NETMSGTYPE_GetTargetMappingsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->mappings, obj->parsed.mappings);
      }

   private:
      // for serialization
      const std::map<uint16_t, NumNodeID>* mappings;

      // for deserialization
      struct {
         std::map<uint16_t, NumNodeID> mappings;
      } parsed;


   public:
      const std::map<uint16_t, NumNodeID>& getMappings() const { return *mappings; }

      std::map<uint16_t, NumNodeID> releaseMappings()
      {
         return mappings == &parsed.mappings
            ? std::move(parsed.mappings)
            : *mappings;
      }
};

#endif /* GETTARGETMAPPINGSRESPMSG_H_ */
