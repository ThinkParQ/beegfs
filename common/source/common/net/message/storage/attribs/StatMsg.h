#ifndef STATMSG_H_
#define STATMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/EntryInfo.h>

#define STATMSG_FLAG_GET_PARENTINFO 1 // caller wants to have parentOwnerNodeID and parentEntryID

class StatMsg : public MirroredMessageBase<StatMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      StatMsg(EntryInfo* entryInfo) : BaseType(NETMSGTYPE_Stat)
      {
         this->entryInfoPtr = entryInfo;
      }

      /**
       * For deserialization only!
       */
      StatMsg() : BaseType(NETMSGTYPE_Stat) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);
      }

      unsigned getSupportedHeaderFeatureFlagsMask() const
      {
         return STATMSG_FLAG_GET_PARENTINFO;
      }

      bool supportsMirroring() const { return true; }

   private:

      // for serialization
      EntryInfo* entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;

   public:

      // getters & setters

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }
};

#endif /*STATMSG_H_*/
