#ifndef SETXATTRMSG_H_
#define SETXATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StatData.h>

class SetXAttrMsg : public MirroredMessageBase<SetXAttrMsg>
{
   friend class AbstractNetMessageFactory;
   friend class TestSerialization;

   public:
      /**
       * @param entryInfo just a reference, do not free it as long as you use this object!
       */
      SetXAttrMsg(EntryInfo* entryInfo, const std::string& name, const CharVector& value, int flags)
            : BaseType(NETMSGTYPE_SetXAttr)
      {
         this->entryInfoPtr = entryInfo;
         this->name = name;
         this->value = value;
         this->flags = flags;
      }

      /**
       * For deserialization only!
       */
      SetXAttrMsg() : BaseType(NETMSGTYPE_SetXAttr) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->name
            % obj->value
            % obj->flags;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->inodeTimestamps;
      }

      private:
         // for serialization
         EntryInfo* entryInfoPtr;

         // for deserialization
         EntryInfo entryInfo;

         std::string name;
         CharVector value;
         int32_t flags;

      protected:
         MirroredTimestamps inodeTimestamps;

      public:
         // getters and setters

         EntryInfo* getEntryInfo(void)
         {
            return &this->entryInfo;
         }

         const std::string& getName(void)
         {
            return this->name;
         }

         const CharVector& getValue(void)
         {
            return this->value;
         }

         int getFlags(void)
         {
            return this->flags;
         }

         bool supportsMirroring() const { return true; }
};

#endif /*SETXATTRMSG_H_*/
