#ifndef GETCLIENTSTATSRESPMSG_H_
#define GETCLIENTSTATSRESPMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/Common.h>


/**
 * After a server received a GetClientStatsMsg request, it will create this object to answer
 * (respond to) the request. With help of the net-message factory the client will
 * receive this object and use it to decode (deserialize) the message.
 */
class GetClientStatsRespMsg : public NetMessage
{
   public:

      /**
       * @param statsVec   - The list has: IP, data1, data2, ..., dataN, IP, data1, ..., dataN
       *                     NOTE: Just a reference, DO NOT free it as long as you use this
       *                           object!
       * @param numOpPerIP - numDataElements tells how many data elements after an IP
       *
       */
      GetClientStatsRespMsg(UInt64Vector* statsVec) :
         NetMessage(NETMSGTYPE_GetClientStatsResp)
      {
         this->statsVec = statsVec;
      }

      GetClientStatsRespMsg() : NetMessage(NETMSGTYPE_GetClientStatsResp)
      {
      }


   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt64Vector(statsVec->size());
      }


   private:
      // for serialization
      UInt64Vector* statsVec; // not owned by this object!
      uint64_t numDataElements; // number of OpCounters per IP in the stats vector

      // for deserialization (calculated on preprocessing)
      unsigned statsVecBufLen; // length of the vector used for our stats elements
      unsigned statsVecElemNum; // number of elements used for our stats
      const char* statsVecStart; // see NETMSG_NICLISTELEM_SIZE for element structure


   public:

      // inliners
      bool parseStatsVector(UInt64Vector* outVec)
      {
         return Serialization::deserializeUInt64Vector(statsVecBufLen, statsVecElemNum, statsVecStart,
            outVec);
      }
};

#endif /* GETCLIENTSTATSRESPMSG_H_ */
