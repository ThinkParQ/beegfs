#pragma once

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/PathInfo.h>
#include <common/Common.h>

// OPENFILERESPMSG_HAS_FILESTATE is a compat feature flag that indicates the metadata server has
// serialized the fileState field as part of the response message. 
#define OPENFILERESPMSG_HAS_FILESTATE     1

class OpenFileRespMsg : public NetMessageSerdes<OpenFileRespMsg>
{
   public:
      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      OpenFileRespMsg(int result, const char* fileHandleID, StripePattern* pattern,
            PathInfo* pathInfo, uint32_t fileVersion = 0, uint8_t fileStateRaw = 0) :
         BaseType(NETMSGTYPE_OpenFileResp),
         fileVersion(fileVersion),
         fileStateRaw(fileStateRaw)
      {
         this->result = result;

         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->pattern = pattern;

         this->pathInfo.set(pathInfo);
      }

      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      OpenFileRespMsg(int result, std::string& fileHandleID, StripePattern* pattern,
            PathInfo* pathInfo, uint32_t fileVersion, uint8_t fileStateRaw = 0) :
         BaseType(NETMSGTYPE_OpenFileResp),
         fileVersion(fileVersion),
         fileStateRaw(fileStateRaw)
      {
         this->result = result;

         this->fileHandleID = fileHandleID.c_str();
         this->fileHandleIDLen = fileHandleID.length();

         this->pattern = pattern;

         this->pathInfo.set(pathInfo);
      }

      /**
       * For deserialization only!
       */
      OpenFileRespMsg() : BaseType(NETMSGTYPE_OpenFileResp) { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->result
            % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
            % obj->pathInfo
            % serdes::backedPtr(obj->pattern, obj->parsed.pattern)
            % obj->fileVersion;
         if (obj->isMsgHeaderCompatFeatureFlagSet(OPENFILERESPMSG_HAS_FILESTATE))
            ctx
               % obj->fileStateRaw;
      }

   private:
      int32_t result;
      unsigned fileHandleIDLen;
      const char* fileHandleID;
      PathInfo pathInfo;
      uint32_t fileVersion;

      // for serialization
      StripePattern* pattern; // not owned by this object!

      // for deserialization
      struct {
         std::unique_ptr<StripePattern> pattern;
      } parsed;
      uint8_t fileStateRaw;

   public:
      StripePattern& getPattern()
      {
         return *pattern;
      }

      int getResult() const
      {
         return result;
      }

      const char* getFileHandleID()
      {
         return fileHandleID;
      }

      PathInfo* getPathInfo()
      {
         return &this->pathInfo;
      }
};

