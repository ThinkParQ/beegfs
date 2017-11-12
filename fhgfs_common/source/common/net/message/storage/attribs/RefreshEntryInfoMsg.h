#ifndef REFRESHENTRYINFOMSG_H_
#define REFRESHENTRYINFOMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StatData.h>

class RefreshEntryInfoMsg : public MirroredMessageBase<RefreshEntryInfoMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      RefreshEntryInfoMsg(EntryInfo* entryInfo) : BaseType(NETMSGTYPE_RefreshEntryInfo)
      {
         this->entryInfoPtr = entryInfo;
      }

      RefreshEntryInfoMsg() : BaseType(NETMSGTYPE_RefreshEntryInfo)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);

         if (obj->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
            ctx % obj->fileTimestamps;
      }

   private:

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;

   protected:
      MirroredTimestamps fileTimestamps;

   public:

      // getters & setters

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      bool supportsMirroring() const { return true; }
};


#endif /* REFRESHENTRYINFOMSG_H_ */
