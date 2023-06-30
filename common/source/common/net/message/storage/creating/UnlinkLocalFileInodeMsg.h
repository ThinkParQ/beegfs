#ifndef UNLINKLOCALFILEINODEMSG_H_
#define UNLINKLOCALFILEINODEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class UnlinkLocalFileInodeMsg : public MirroredMessageBase<UnlinkLocalFileInodeMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param delEntryInfo just a reference, so do not free it as long as you use this object!
       */
      UnlinkLocalFileInodeMsg(EntryInfo* delEntryInfo) :
         BaseType(NETMSGTYPE_UnlinkLocalFileInode), delEntryInfoPtr(delEntryInfo)
      {
      }

      UnlinkLocalFileInodeMsg() : BaseType(NETMSGTYPE_UnlinkLocalFileInode)
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

#endif /* UNLINKLOCALFILEINODEMSG_H_ */
