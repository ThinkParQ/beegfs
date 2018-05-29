#ifndef UNLINKLOCALFILEMSG_H_
#define UNLINKLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/PathInfo.h>


#define UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR        1 /* given targetID is a buddymirrorgroup ID */
#define UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND 2 /* secondary of group, otherwise primary */


class UnlinkLocalFileMsg : public NetMessageSerdes<UnlinkLocalFileMsg>
{
   friend class AbstractNetMessageFactory;

   public:


      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       */
      UnlinkLocalFileMsg(std::string& entryID, uint16_t targetID, PathInfo* pathInfo) :
         BaseType(NETMSGTYPE_UnlinkLocalFile)
      {
         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();

         this->targetID = targetID;

         this->pathInfoPtr = pathInfo;
      }

      /**
       * For deserialization only!
       */
      UnlinkLocalFileMsg() : BaseType(NETMSGTYPE_UnlinkLocalFile) { }

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
         return UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR | UNLINKLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND;
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

      // getters & setters

      const char* getEntryID() const
      {
         return entryID;
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


#endif /*UNLINKLOCALFILEMSG_H_*/
