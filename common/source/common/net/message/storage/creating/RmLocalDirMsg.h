#ifndef RMLOCALDIRMSG_H_
#define RMLOCALDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class RmLocalDirMsg : public MirroredMessageBase<RmLocalDirMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param delEntryInfo just a reference, so do not free it as long as you use this object!
       */
      RmLocalDirMsg(EntryInfo* delEntryInfo) :
         BaseType(NETMSGTYPE_RmLocalDir)
      {
         this->delEntryInfoPtr = delEntryInfo;

      }

      RmLocalDirMsg() : BaseType(NETMSGTYPE_RmLocalDir)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::backedPtr(obj->delEntryInfoPtr, obj->delEntryInfo);
      }

   private:

      // for serialization
      EntryInfo* delEntryInfoPtr;

      // for deserialization
      EntryInfo delEntryInfo;

   public:
      // getters & setters
      EntryInfo* getDelEntryInfo(void)
      {
         return &this->delEntryInfo;
      }

      bool supportsMirroring() const { return true; }
};

#endif /*RMLOCALDIRMSG_H_*/
