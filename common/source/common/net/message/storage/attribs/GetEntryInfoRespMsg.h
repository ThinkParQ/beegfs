#ifndef GETENTRYINFORESPMSG_H_
#define GETENTRYINFORESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/StorageErrors.h>
#include <common/storage/PathInfo.h>
#include <common/Common.h>

/**
 * Retrieve information about an entry, such as the stripe pattern or mirroring settings.
 * This is typically used by "fhgfs-ctl --getentryinfo".
 */
class GetEntryInfoRespMsg : public NetMessageSerdes<GetEntryInfoRespMsg>
{
   public:
      /**
       * @param mirrorNodeID ID of metadata mirror node
       */
      GetEntryInfoRespMsg(FhgfsOpsErr result, StripePattern* pattern, uint16_t mirrorNodeID,
         PathInfo* pathInfoPtr) : BaseType(NETMSGTYPE_GetEntryInfoResp)
      {
         this->result = result;
         this->pattern = pattern;
         this->mirrorNodeID = mirrorNodeID;
         this->pathInfoPtr = pathInfoPtr;
      }

      /**
       * For deserialization only
       */
      GetEntryInfoRespMsg() : BaseType(NETMSGTYPE_GetEntryInfoResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % serdes::backedPtr(obj->pattern, obj->parsed.pattern)
            % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo)
            % obj->mirrorNodeID;
      }

   private:
      int32_t result;
      uint16_t mirrorNodeID; // metadata mirror node (0 means "none")

      // for serialization
      StripePattern* pattern; // not owned by this object!
      PathInfo* pathInfoPtr;  // not owned by this object!

      // for deserialization
      struct {
         std::unique_ptr<StripePattern> pattern;
      } parsed;
      PathInfo pathInfo;

   public:
      StripePattern& getPattern()
      {
         return *pattern;
      }

      FhgfsOpsErr getResult() const
      {
         return (FhgfsOpsErr)result;
      }

      uint16_t getMirrorNodeID() const
      {
         return mirrorNodeID;
      }

      PathInfo* getPathInfo()
      {
         return &this->pathInfo;
      }

};


#endif /*GETENTRYINFORESPMSG_H_*/
