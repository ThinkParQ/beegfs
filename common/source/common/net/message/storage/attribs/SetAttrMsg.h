#ifndef SETATTRMSG_H_
#define SETATTRMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/FileEvent.h>
#include <common/storage/Path.h>
#include <common/storage/StatData.h>
#include <common/storage/StorageDefinitions.h>


#define SETATTRMSG_FLAG_USE_QUOTA            1  /* if the message contains quota informations */
#define SETATTRMSG_FLAG_HAS_EVENT            4  /* contains file event logging information */
#define SETATTRMSG_FLAG_INCR_NLINKCNT        8  /* increment nlink count */
#define SETATTRMSG_FLAG_DECR_NLINKCNT        16 /* decrement nlink count */


class SetAttrMsg : public MirroredMessageBase<SetAttrMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       * @param validAttribs a combination of SETATTR_CHANGE_...-Flags
       */
      SetAttrMsg(EntryInfo *entryInfo, int validAttribs, SettableFileAttribs* attribs)
       : BaseType(NETMSGTYPE_SetAttr),
         validAttribs(validAttribs),
         attribs(*attribs),
         entryInfoPtr(entryInfo)
      { }

      /**
       * For deserialization only!
       */
      SetAttrMsg() : BaseType(NETMSGTYPE_SetAttr) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->validAttribs
            % obj->attribs.mode
            % obj->attribs.modificationTimeSecs
            % obj->attribs.lastAccessTimeSecs
            % obj->attribs.userID
            % obj->attribs.groupID
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->inodeTimestamps;

         if (obj->isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_HAS_EVENT))
            ctx % obj->fileEvent;
      }

   private:
      int32_t validAttribs;
      SettableFileAttribs attribs;
      FileEvent fileEvent;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;

   protected:
      MirroredTimestamps inodeTimestamps;

   public:
      // getters & setters
      int getValidAttribs()
      {
         return validAttribs;
      }

      SettableFileAttribs* getAttribs()
      {
         return &attribs;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      const FileEvent* getFileEvent() const
      {
         if (isMsgHeaderFeatureFlagSet(SETATTRMSG_FLAG_HAS_EVENT))
            return &fileEvent;
         else
            return nullptr;
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return SETATTRMSG_FLAG_USE_QUOTA |
            SETATTRMSG_FLAG_HAS_EVENT |
            SETATTRMSG_FLAG_INCR_NLINKCNT |
            SETATTRMSG_FLAG_DECR_NLINKCNT;
      }

      bool supportsMirroring() const { return true; }
};

#endif /*SETATTRMSG_H_*/
