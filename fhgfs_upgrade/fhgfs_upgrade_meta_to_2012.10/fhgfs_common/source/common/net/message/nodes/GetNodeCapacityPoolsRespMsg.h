#ifndef GETNODECAPACITYPOOLSRESP_H_
#define GETNODECAPACITYPOOLSRESP_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/Common.h>


class GetNodeCapacityPoolsRespMsg : public NetMessage
{
   public:

      /**
       * @param listNormal just a reference, so do not free it as long as you use this object!
       * @param listLow just a reference, so do not free it as long as you use this object!
       * @param listEmergency just a reference, so do not free it as long as you use this object!
       */
      GetNodeCapacityPoolsRespMsg(UInt16List* listNormal, UInt16List* listLow,
         UInt16List* listEmergency) : NetMessage(NETMSGTYPE_GetNodeCapacityPoolsResp)
      {
         this->listNormal = listNormal;
         this->listLow = listLow;
         this->listEmergency = listEmergency;
      }

      GetNodeCapacityPoolsRespMsg() : NetMessage(NETMSGTYPE_GetNodeCapacityPoolsResp)
      {
      }


   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt16List(listNormal) +
            Serialization::serialLenUInt16List(listLow) +
            Serialization::serialLenUInt16List(listEmergency);
      }


   private:
      // for serialization
      UInt16List* listNormal; // not owned by this object!
      UInt16List* listLow; // not owned by this object!
      UInt16List* listEmergency; // not owned by this object!

      // for deserialization
      unsigned listNormalElemNum;
      const char* listNormalStart;
      unsigned listNormalBufLen;
      unsigned listLowElemNum;
      const char* listLowStart;
      unsigned listLowBufLen;
      unsigned listEmergencyElemNum;
      const char* listEmergencyStart;
      unsigned listEmergencyBufLen;


   public:

      // inliners

      void parseLists(UInt16List* outListNormal, UInt16List* outListLow,
         UInt16List* outListEmergency)
      {
         Serialization::deserializeUInt16List(listNormalBufLen, listNormalElemNum, listNormalStart,
            outListNormal);
         Serialization::deserializeUInt16List(listLowBufLen, listLowElemNum, listLowStart,
            outListLow);
         Serialization::deserializeUInt16List(listEmergencyBufLen, listEmergencyElemNum,
            listEmergencyStart, outListEmergency);
      }

};

#endif /* GETNODECAPACITYPOOLSRESP_H_ */
