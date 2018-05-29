#ifndef REMOVEXATTRMSG_H_
#define REMOVEXATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StatData.h>

class RemoveXAttrMsg : public MirroredMessageBase<RemoveXAttrMsg>
{
   friend class AbstractNetMessageFactory;

   public:
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      RemoveXAttrMsg(EntryInfo* entryInfo, const std::string& name)
            : BaseType(NETMSGTYPE_RemoveXAttr)
      {
         this->entryInfoPtr = entryInfo;
         this->name = name;
      }

      /**
       * For deserialization only!
       */
      RemoveXAttrMsg() : BaseType(NETMSGTYPE_RemoveXAttr) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->name;

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->inodeTimestamps;
      }

   private:

      // for serialization
      EntryInfo* entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;

      std::string name;

   protected:
      MirroredTimestamps inodeTimestamps;

   public:
      // getters and setters

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      const std::string& getName(void)
      {
         return this->name;
      }

      bool supportsMirroring() const { return true; }
};

#endif /*REMOVEXATTRMSG_H_*/
