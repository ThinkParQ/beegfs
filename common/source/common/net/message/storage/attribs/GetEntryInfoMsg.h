#ifndef GETENTRYINFOMSG_H_
#define GETENTRYINFOMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class GetEntryInfoMsg : public MirroredMessageBase<GetEntryInfoMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param path just a reference, so do not free it as long as you use this object!
       */
      GetEntryInfoMsg(EntryInfo* entryInfo) : BaseType(NETMSGTYPE_GetEntryInfo)
      {
         this->entryInfoPtr = entryInfo;
      }

      GetEntryInfoMsg() : BaseType(NETMSGTYPE_GetEntryInfo)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo);
      }

   private:
      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;

   public:
      // getters & setters
      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }
};

#endif /*GETENTRYINFOMSG_H_*/
