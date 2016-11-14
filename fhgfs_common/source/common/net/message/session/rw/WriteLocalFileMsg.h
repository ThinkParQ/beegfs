#ifndef WRITELOCALFILEMSG_H_
#define WRITELOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/PathInfo.h>


#define WRITELOCALFILEMSG_FLAG_SESSION_CHECK        1 /* if session check should be done */
#define WRITELOCALFILEMSG_FLAG_USE_QUOTA            2 /* if msg contains quota info */
#define WRITELOCALFILEMSG_FLAG_DISABLE_IO           4 /* disable write syscall for net bench mode */
#define WRITELOCALFILEMSG_FLAG_BUDDYMIRROR          8 /* given targetID is a buddymirrorgroup ID */
#define WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND  16 /* secondary of group, otherwise primary */
#define WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_FORWARD 32 /* forward msg to secondary */


class WriteLocalFileMsg : public NetMessageSerdes<WriteLocalFileMsg>
{
   public:
      
      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param pathInfo just a reference, so do not free it as long as you use this object!
       */
      WriteLocalFileMsg(const NumNodeID clientNumID, const char* fileHandleID,
         const uint16_t targetID, const PathInfo* pathInfo, const unsigned accessFlags,
         const int64_t offset, const int64_t count) : BaseType(NETMSGTYPE_WriteLocalFile)
      {
         this->clientNumID = clientNumID;
         
         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->targetID = targetID;

         this->pathInfoPtr = pathInfo;

         this->accessFlags = accessFlags;

         this->offset = offset;
         this->count = count;
      }

      /**
       * For deserialization only!
       */
      WriteLocalFileMsg() : BaseType(NETMSGTYPE_WriteLocalFile) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->offset
            % obj->count
            % obj->accessFlags;

         if(obj->isMsgHeaderFeatureFlagSet(WRITELOCALFILEMSG_FLAG_USE_QUOTA))
            ctx
               % obj->userID
               % obj->groupID;

         ctx
            % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
            % obj->clientNumID
            % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo)
            % obj->targetID;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return WRITELOCALFILEMSG_FLAG_SESSION_CHECK | WRITELOCALFILEMSG_FLAG_USE_QUOTA |
            WRITELOCALFILEMSG_FLAG_DISABLE_IO | WRITELOCALFILEMSG_FLAG_BUDDYMIRROR |
            WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND | WRITELOCALFILEMSG_FLAG_BUDDYMIRROR_FORWARD;
      }


   private:
      int64_t offset;
      int64_t count;
      uint32_t accessFlags;
      const char* fileHandleID;
      unsigned fileHandleIDLen;
      NumNodeID clientNumID;
      uint16_t targetID;

      uint32_t userID;
      uint32_t groupID;

      // for serialization
      const PathInfo* pathInfoPtr;

      // for deserilization
      PathInfo pathInfo;

   public:
      // getters & setters

      NumNodeID getClientNumID() const
      {
         return clientNumID;
      }
      
      const char* getFileHandleID() const
      {
         return fileHandleID;
      }
      
      uint16_t getTargetID() const
      {
         return targetID;
      }

      unsigned getAccessFlags() const
      {
         return accessFlags;
      }
      
      int64_t getOffset() const
      {
         return offset;
      }
      
      int64_t getCount() const
      {
         return count;
      }
      
      PathInfo* getPathInfo ()
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
         addMsgHeaderFeatureFlag(WRITELOCALFILEMSG_FLAG_USE_QUOTA);

         this->userID = userID;
         this->groupID = groupID;
      }

      // inliners

      void sendData(const char* data, Socket* sock)
      {
         sock->send(data, count, 0);
      }


};

#endif /*WRITELOCALFILEMSG_H_*/
