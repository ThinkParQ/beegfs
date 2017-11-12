#ifndef RMCHUNKPATHSMSG_H
#define RMCHUNKPATHSMSG_H

#include <common/net/message/NetMessage.h>
#include <common/toolkit/serialization/Serialization.h>

#define RMCHUNKPATHSMSG_FLAG_BUDDYMIRROR        1 /* given targetID is a buddymirrorgroup ID */

class RmChunkPathsMsg : public NetMessageSerdes<RmChunkPathsMsg>
{
   public:
      RmChunkPathsMsg(uint16_t targetID, StringList* relativePaths) :
         BaseType(NETMSGTYPE_RmChunkPaths)
      {
         this->targetID = targetID;
         this->relativePaths = relativePaths;
      }

      RmChunkPathsMsg() : BaseType(NETMSGTYPE_RmChunkPaths)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->targetID
            % serdes::backedPtr(obj->relativePaths, obj->parsed.relativePaths);
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return RMCHUNKPATHSMSG_FLAG_BUDDYMIRROR;
      }

   private:
      uint16_t targetID;

      // for serialization
      StringList* relativePaths;

      // for deserialization
      struct {
         StringList relativePaths;
      } parsed;

   public:
      uint16_t getTargetID() const
      {
         return targetID;
      }

      StringList& getRelativePaths()
      {
         return *relativePaths;
      }
};


#endif /*RMCHUNKPATHSMSG_H*/
