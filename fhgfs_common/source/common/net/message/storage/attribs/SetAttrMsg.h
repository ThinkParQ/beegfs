#ifndef SETATTRMSG_H_
#define SETATTRMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/Path.h>
#include <common/storage/StatData.h>
#include <common/storage/StorageDefinitions.h>


#define SETATTRMSG_FLAG_USE_QUOTA            1 /* if the message contains quota informations */

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
         attribs(*attribs)
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
      }

   private:
      int32_t validAttribs;
      SettableFileAttribs attribs;

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

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return SETATTRMSG_FLAG_USE_QUOTA;
      }

      bool supportsMirroring() const { return true; }
};

#endif /*SETATTRMSG_H_*/
