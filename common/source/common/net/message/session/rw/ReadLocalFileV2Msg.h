#ifndef READLOCALFILEV2MSG_H_
#define READLOCALFILEV2MSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/PathInfo.h>


#define READLOCALFILEMSG_FLAG_SESSION_CHECK       1 /* if session check infos should be done */
#define READLOCALFILEMSG_FLAG_DISABLE_IO          2 /* disable read syscall for net bench */
#define READLOCALFILEMSG_FLAG_BUDDYMIRROR         4 /* given targetID is a buddymirrorgroup ID */
#define READLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND  8 /* secondary of group, otherwise primary */


class ReadLocalFileV2MsgBase
{
   public:

      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      ReadLocalFileV2MsgBase(NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID,
         PathInfo* pathInfoPtr, unsigned accessFlags, int64_t offset, int64_t count)
      {
         this->clientNumID = clientNumID;
         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->targetID = targetID;

         this->pathInfoPtr = pathInfoPtr;

         this->accessFlags = accessFlags;

         this->offset = offset;
         this->count = count;
      }

      /**
       * For deserialization only!
       */
      ReadLocalFileV2MsgBase() {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->offset
            % obj->count
            % obj->accessFlags
            % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
            % obj->clientNumID
            % serdes::backedPtr(obj->pathInfoPtr, obj->pathInfo)
            % obj->targetID;
      }

   protected:
      int64_t offset;
      int64_t count;
      uint32_t accessFlags;
      const char* fileHandleID;
      unsigned fileHandleIDLen;
      NumNodeID clientNumID;
      uint16_t targetID;

      // for serialization
      PathInfo* pathInfoPtr;

      // for deserialization
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

      PathInfo* getPathInfo()
      {
         return &this->pathInfo;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return READLOCALFILEMSG_FLAG_SESSION_CHECK | READLOCALFILEMSG_FLAG_DISABLE_IO |
            READLOCALFILEMSG_FLAG_BUDDYMIRROR | READLOCALFILEMSG_FLAG_BUDDYMIRROR_SECOND;
      }

      bool isMsgValid() const
      {
         return true;
      }
};

class ReadLocalFileV2Msg : public ReadLocalFileV2MsgBase, public NetMessageSerdes<ReadLocalFileV2Msg>
{
   public:
      /**
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      ReadLocalFileV2Msg(NumNodeID clientNumID, const char* fileHandleID, uint16_t targetID,
         PathInfo* pathInfoPtr, unsigned accessFlags, int64_t offset, int64_t count) :
         ReadLocalFileV2MsgBase(clientNumID, fileHandleID, targetID, pathInfoPtr, accessFlags,
            offset, count),
         BaseType(NETMSGTYPE_ReadLocalFileV2) {}

      /**
       * For deserialization only!
       */
      ReadLocalFileV2Msg() :
         ReadLocalFileV2MsgBase(),
         BaseType(NETMSGTYPE_ReadLocalFileV2) {}

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return ReadLocalFileV2MsgBase::getSupportedHeaderFeatureFlagsMask();
      }
};

#endif /*READLOCALFILEV2MSG_H_*/
