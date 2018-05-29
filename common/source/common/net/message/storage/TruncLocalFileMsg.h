#ifndef TRUNCLOCALFILEMSG_H_
#define TRUNCLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/PathInfo.h>
#include <common/storage/EntryInfo.h>


#define TRUNCLOCALFILEMSG_FLAG_NODYNAMICATTRIBS   1 /* skip retrieval of current dyn attribs */
#define TRUNCLOCALFILEMSG_FLAG_USE_QUOTA          2 /* if the message contains quota informations */
#define TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR        4 /* given targetID is a buddymirrorgroup ID */
#define TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND 8 /* secondary of group, otherwise primary */

class TruncLocalFileMsg : public NetMessageSerdes<TruncLocalFileMsg>
{
   public:

      /**
       * @param entryID just a reference, so do not free it as long as you use this object!
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       */
      TruncLocalFileMsg(int64_t filesize, std::string& entryID, uint16_t targetID,
         PathInfo* pathInfo) :
         BaseType(NETMSGTYPE_TruncLocalFile)
      {
         this->filesize = filesize;

         this->entryID = entryID.c_str();
         this->entryIDLen = entryID.length();

         this->targetID = targetID;

         this->pathInfoPtr = pathInfo;
      }

      TruncLocalFileMsg() : BaseType(NETMSGTYPE_TruncLocalFile) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->filesize;

         if(obj->isMsgHeaderFeatureFlagSet(TRUNCLOCALFILEMSG_FLAG_USE_QUOTA) )
         {
            ctx
               % obj->userID
               % obj->groupID;
         }

         ctx
            % serdes::rawString(obj->entryID, obj->entryIDLen, 4)
            % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo)
            % obj->targetID;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return TRUNCLOCALFILEMSG_FLAG_NODYNAMICATTRIBS | TRUNCLOCALFILEMSG_FLAG_USE_QUOTA |
            TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR | TRUNCLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND;
      }


   private:
      int64_t filesize;

      unsigned entryIDLen;
      const char* entryID;

      uint16_t targetID;

      uint32_t userID;
      uint32_t groupID;

      // for serialization
      PathInfo* pathInfoPtr;

      // for deserialization
      PathInfo pathInfo;


   public:

      // getters & setters

      int64_t getFilesize() const
      {
         return filesize;
      }

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

      unsigned getUserID() const
      {
         return userID;
      }

      unsigned getGroupID() const
      {
         return groupID;
      }

      void setUserdataForQuota(unsigned userID, unsigned groupID)
      {
         this->addMsgHeaderFeatureFlag(TRUNCLOCALFILEMSG_FLAG_USE_QUOTA);

         this->userID = userID;
         this->groupID = groupID;
      }
};

#endif /*TRUNCLOCALFILEMSG_H_*/
