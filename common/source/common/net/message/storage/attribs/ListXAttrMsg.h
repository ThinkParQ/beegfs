#ifndef LISTXATTRMSG_H_
#define LISTXATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class ListXAttrMsg : public MirroredMessageBase<ListXAttrMsg>
{
   friend class AbstractNetMessageFactory;

   public:
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      ListXAttrMsg(EntryInfo* entryInfo, int size) : BaseType(NETMSGTYPE_ListXAttr),
         entryInfoPtr(entryInfo), size(size)
      {
      }

      /**
       * For deserialization only!
       */
      ListXAttrMsg() : BaseType(NETMSGTYPE_ListXAttr) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->size;
      }

      bool supportsMirroring() const { return true; }

   private:

      // for serialization
      EntryInfo* entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;

      int32_t size;

   public:
      // getters and setters

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      int getSize(void) const
      {
         return this->size;
      }
};

#endif /*LISTXATTRMSG_H_*/
