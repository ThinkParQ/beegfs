#ifndef COMMON_SETLOCALATTRMSG_H_
#define COMMON_SETLOCALATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/PathInfo.h>
#include <common/storage/StorageDefinitions.h>


#define SETLOCALATTRMSG_FLAG_USE_QUOTA            1 // if the message contains quota informations
#define SETLOCALATTRMSG_FLAG_BUDDYMIRROR          2 // given targetID is a buddymirrorgroup ID
#define SETLOCALATTRMSG_FLAG_BUDDYMIRROR_SECOND   4 // secondary of group, otherwise primary

class SetLocalAttrMsg : public NetMessageSerdes<SetLocalAttrMsg>
{
   friend class AbstractNetMessageFactory;

   public:
      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       * @param validAttribs a combination of SETATTR_CHANGE_...-Flags
       */
      SetLocalAttrMsg(std::string& entryID, uint16_t targetID, PathInfo* pathInfo, int validAttribs,
         SettableFileAttribs* attribs, bool enableCreation) : BaseType(NETMSGTYPE_SetLocalAttr)
      {
         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();
         this->targetID = targetID;
         this->pathInfoPtr = pathInfo;

         this->validAttribs = validAttribs;
         this->attribs = *attribs;
         this->enableCreation = enableCreation;
      }

      /**
       * For deserialization only!
       */
      SetLocalAttrMsg() : BaseType(NETMSGTYPE_SetLocalAttr) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->attribs.modificationTimeSecs
            % obj->attribs.lastAccessTimeSecs
            % obj->validAttribs
            % obj->attribs.mode
            % obj->attribs.userID
            % obj->attribs.groupID
            % serdes::rawString(obj->entryID, obj->entryIDLen, 4)
            % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo)
            % obj->targetID
            % obj->enableCreation;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return SETLOCALATTRMSG_FLAG_USE_QUOTA | SETLOCALATTRMSG_FLAG_BUDDYMIRROR |
            SETLOCALATTRMSG_FLAG_BUDDYMIRROR_SECOND;
      }



   private:
      unsigned entryIDLen;
      const char* entryID;
      uint16_t targetID;
      int32_t validAttribs;
      SettableFileAttribs attribs;
      bool enableCreation;

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

      int getValidAttribs() const
      {
         return validAttribs;
      }

      const SettableFileAttribs* getAttribs() const
      {
         return &attribs;
      }

      bool getEnableCreation() const
      {
         return enableCreation;
      }

      const PathInfo* getPathInfo() const
      {
         return &this->pathInfo;
      }

};

#endif /*COMMON_SETLOCALATTRMSG_H_*/
