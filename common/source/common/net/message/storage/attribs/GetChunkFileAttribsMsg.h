#ifndef GETCHUNKFILEATTRIBSMSG_H_
#define GETCHUNKFILEATTRIBSMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/PathInfo.h>


#define GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR        1 /* given targetID is a buddymirrorgroup ID */
#define GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR_SECOND 2 /* secondary of group, otherwise primary */


class GetChunkFileAttribsMsg : public NetMessageSerdes<GetChunkFileAttribsMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param pathInfo: Only as pointer, not owned by this object!
       */
      GetChunkFileAttribsMsg(const std::string& entryID, uint16_t targetID, PathInfo* pathInfo) :
         BaseType(NETMSGTYPE_GetChunkFileAttribs)
      {
         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();

         this->targetID = targetID;

         this->pathInfoPtr = pathInfo;
      }

      /**
       * For deserialization only
       */
      GetChunkFileAttribsMsg() : BaseType(NETMSGTYPE_GetChunkFileAttribs)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::rawString(obj->entryID, obj->entryIDLen, 4)
            % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo)
            % obj->targetID;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR |
            GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR_SECOND;
      }

   private:
      unsigned entryIDLen;
      const char* entryID;
      uint16_t targetID;

      // for serialization
      PathInfo* pathInfoPtr;

      // for deserialization
      PathInfo pathInfo;


   public:

      // inliners
      const char* getEntryID() const
      {
         return entryID;
      }

      // getters & setters
      uint16_t getTargetID() const
      {
         return targetID;
      }

      PathInfo* getPathInfo()
      {
         return &this->pathInfo;
      }


};

#endif /*GETCHUNKFILEATTRIBSMSG_H_*/
