#ifndef MAPTARGETSRESPMSG_H_
#define MAPTARGETSRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>


class MapTargetsRespMsg : public NetMessageSerdes<MapTargetsRespMsg>
{
   public:
      /**
       * @param resultVec vector<FhgfsOpsErr> indicating the map result of each target sent
       */
      MapTargetsRespMsg(FhgfsOpsErrVec* resultVec):
            BaseType(NETMSGTYPE_MapTargetsResp), resultVec(resultVec)
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
            % serdes::backedPtr(obj->resultVec, obj->parsed.resultVec);
      }

      const FhgfsOpsErrVec& getResultVec() const
      {
         return *resultVec;
      }

   private:
      // for serialization
      FhgfsOpsErrVec* resultVec;

      // for deserialization
      struct
      {
         FhgfsOpsErrVec resultVec;
      } parsed;
};


#endif /* MAPTARGETSRESPMSG_H_ */
