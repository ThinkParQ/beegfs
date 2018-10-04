#ifndef CLOSECHUNKFILEMSG_H_
#define CLOSECHUNKFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/PathInfo.h>


#define CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS    1 /* skip retrieval of current dyn attribs */
#define CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR         4 /* given targetID is a buddymirrorgroup ID */
#define CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND  8 /* secondary of group, otherwise primary */


class CloseChunkFileMsg : public NetMessageSerdes<CloseChunkFileMsg>
{
   public:
      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       */
      CloseChunkFileMsg(const NumNodeID sessionID, const std::string& fileHandleID,
         const uint16_t targetID, const PathInfo* pathInfo) :
         BaseType(NETMSGTYPE_CloseChunkFile)
      {
         this->sessionID = sessionID;

         this->fileHandleID = fileHandleID.c_str();
         this->fileHandleIDLen = fileHandleID.length();

         this->targetID = targetID;

         this->pathInfoPtr = pathInfo;
      }

      /**
       * For deserialization only!
       */
      CloseChunkFileMsg() : BaseType(NETMSGTYPE_CloseChunkFile) { }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->sessionID
            % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
            % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo);

         ctx % obj->targetID;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS |
            CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR | CLOSECHUNKFILEMSG_FLAG_BUDDYMIRROR_SECOND;
      }


   private:
      NumNodeID sessionID;
      unsigned fileHandleIDLen;
      const char* fileHandleID;
      uint16_t targetID;

      // for serialization
      const PathInfo *pathInfoPtr;

      // for deserialization
      PathInfo pathInfo;

   public:
      // getters & setters

      NumNodeID getSessionID() const
      {
         return sessionID;
      }

      const char* getFileHandleID() const
      {
         return fileHandleID;
      }

      uint16_t getTargetID() const
      {
         return targetID;
      }

      const PathInfo* getPathInfo() const
      {
         return &this->pathInfo;
      }
};


#endif /*CLOSECHUNKFILEMSG_H_*/
