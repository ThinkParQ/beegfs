#ifndef RMCHUNKPATHSRESPMSG_H
#define RMCHUNKPATHSRESPMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>

class RmChunkPathsRespMsg : public NetMessageSerdes<RmChunkPathsRespMsg>
{
   public:
      RmChunkPathsRespMsg(StringList* failedPaths) : BaseType(NETMSGTYPE_RmChunkPathsResp)
      {
         this->failedPaths = failedPaths;
      }

      RmChunkPathsRespMsg() : BaseType(NETMSGTYPE_RmChunkPathsResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->failedPaths, obj->parsed.failedPaths);
      }

   private:
      // for serialization
      StringList* failedPaths;

      // for deserialization
      struct {
         StringList failedPaths;
      } parsed;

   public:
      StringList& getFailedPaths()
      {
         return *failedPaths;
      }
};


#endif /*RMCHUNKPATHSRESPMSG_H*/
