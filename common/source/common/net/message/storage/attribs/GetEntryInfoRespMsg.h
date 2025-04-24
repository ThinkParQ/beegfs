#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/RemoteStorageTarget.h>
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
         PathInfo* pathInfoPtr, RemoteStorageTarget* rst, uint32_t numSessionsRead = 0,
         uint32_t numSessionsWrite = 0, uint8_t dataState = 0) : BaseType(NETMSGTYPE_GetEntryInfoResp)
      {
         this->result = result;
         this->pattern = pattern;
         this->mirrorNodeID = mirrorNodeID;
         this->numSessionsRead = numSessionsRead;
         this->numSessionsWrite = numSessionsWrite;
         this->fileDataState = dataState;
         this->pathInfoPtr = pathInfoPtr;
         this->rstPtr = rst;
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
            % serdes::backedPtr(obj->rstPtr, obj->rst)
            % obj->mirrorNodeID
            % obj->numSessionsRead
            % obj->numSessionsWrite
            % obj->fileDataState;
      }

   private:
      int32_t result;

      uint16_t mirrorNodeID; // metadata mirror node (0 means "none")

      uint32_t numSessionsRead;     // only applicable for regular files
      uint32_t numSessionsWrite;    // only applicable for regular files
      uint8_t  fileDataState;       // only applicable for regular files

      // for serialization
      StripePattern* pattern;       // not owned by this object!
      PathInfo* pathInfoPtr;        // not owned by this object!
      RemoteStorageTarget* rstPtr;  // not owned by this object!

      // for deserialization
      RemoteStorageTarget rst;
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

      RemoteStorageTarget* getRemoteStorageTarget()
      {
         return &this->rst;
      }

      uint32_t getNumSessionsRead() const
      {
         return numSessionsRead;
      }

      uint32_t getNumSessionsWrite() const
      {
         return numSessionsWrite;
      }

      uint8_t getFileDataState() const
      {
         return fileDataState;
      }
};


