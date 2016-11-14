#ifndef GETNODECAPACITYPOOLSRESP_H_
#define GETNODECAPACITYPOOLSRESP_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/Common.h>


class GetNodeCapacityPoolsRespMsg : public NetMessageSerdes<GetNodeCapacityPoolsRespMsg>
{
   public:

      /**
       * @param listNormal just a reference, so do not free it as long as you use this object!
       * @param listLow just a reference, so do not free it as long as you use this object!
       * @param listEmergency just a reference, so do not free it as long as you use this object!
       */
      GetNodeCapacityPoolsRespMsg(UInt16List* listNormal, UInt16List* listLow,
         UInt16List* listEmergency) : BaseType(NETMSGTYPE_GetNodeCapacityPoolsResp)
      {
         this->listNormal = listNormal;
         this->listLow = listLow;
         this->listEmergency = listEmergency;
      }

      GetNodeCapacityPoolsRespMsg() : BaseType(NETMSGTYPE_GetNodeCapacityPoolsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->listNormal, obj->parsed.listNormal)
            % serdes::backedPtr(obj->listLow, obj->parsed.listLow)
            % serdes::backedPtr(obj->listEmergency, obj->parsed.listEmergency);
      }

   private:
      // for serialization
      UInt16List* listNormal; // not owned by this object!
      UInt16List* listLow; // not owned by this object!
      UInt16List* listEmergency; // not owned by this object!

      // for deserialization
      struct {
         UInt16List listNormal;
         UInt16List listLow;
         UInt16List listEmergency;
      } parsed;

   public:
      UInt16List& getNormal()
      {
         return *listNormal;
      }

      UInt16List& getLow()
      {
         return *listLow;
      }

      UInt16List& getEmergency()
      {
         return *listEmergency;
      }
};

#endif /* GETNODECAPACITYPOOLSRESP_H_ */
