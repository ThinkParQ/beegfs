#ifndef MOVINGFILEINSERTMSG_H_
#define MOVINGFILEINSERTMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperXAttr.h>

#define MOVINGFILEINSERTMSG_FLAG_HAS_XATTRS         1

/**
 * Used to tell a remote metadata server a file is to be moved between directories
 */
class MovingFileInsertMsg : public MirroredMessageBase<MovingFileInsertMsg>
{
   friend class AbstractNetMessageFactory;

   public:
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       * @param serialBuf serialized file;
       * just a reference, so do not free it as long as you use this object!
       */
      MovingFileInsertMsg(EntryInfo* fromFileInfo, EntryInfo* toDirInfo, std::string& newName,
         const char* serialBuf, unsigned serialBufLen) :  BaseType(NETMSGTYPE_MovingFileInsert)
      {
         this->fromFileInfoPtr   = fromFileInfo;
         this->newName           = newName;
         this->toDirInfoPtr      = toDirInfo;

         this->serialBuf         = serialBuf;
         this->serialBufLen      = serialBufLen;
      }

      MovingFileInsertMsg() : BaseType(NETMSGTYPE_MovingFileInsert)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->fromFileInfoPtr, obj->fromFileInfo)
            % serdes::backedPtr(obj->toDirInfoPtr, obj->toDirInfo)
            % serdes::stringAlign4(obj->newName)
            % obj->serialBufLen
            % serdes::rawBlock(obj->serialBuf, obj->serialBufLen);

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx
               % obj->dirTimestamps
               % obj->fileTimestamps;
      }

      bool supportsMirroring() const { return true; }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return MOVINGFILEINSERTMSG_FLAG_HAS_XATTRS;
      }

   private:
      EntryInfo  fromFileInfo; // for deserialization
      EntryInfo* fromFileInfoPtr; // for serialization

      std::string newName;

      const char* serialBuf;
      uint32_t serialBufLen;

      EntryInfo* toDirInfoPtr; // for serialization
      EntryInfo  toDirInfo; // for deserialization

   protected:
      MirroredTimestamps dirTimestamps;
      MirroredTimestamps fileTimestamps;

   public:

      // getters & setters
      const char* getSerialBuf()
      {
         return serialBuf;
      }

      uint32_t getSerialBufLen() const { return serialBufLen; }

      EntryInfo* getToDirInfo()
      {
         return &this->toDirInfo;
      }

      std::string getNewName()
      {
         return this->newName;
      }

      EntryInfo* getFromFileInfo()
      {
         return &this->fromFileInfo;
      }

      void registerStreamoutHook(RequestResponseArgs& rrArgs, MsgHelperXAttr::StreamXAttrState& ctx)
      {
         addMsgHeaderFeatureFlag(MOVINGFILEINSERTMSG_FLAG_HAS_XATTRS);
         rrArgs.sendExtraData = &MsgHelperXAttr::StreamXAttrState::streamXattrFn;
         rrArgs.extraDataContext = &ctx;
      }
};

#endif /*MOVINGFILEINSERTMSG_H_*/
