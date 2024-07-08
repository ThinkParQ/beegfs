#ifndef GETXATTRMSG_H_
#define GETXATTRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>

class GetXAttrMsg : public MirroredMessageBase<GetXAttrMsg>
{
   friend class AbstractNetMessageFactory;

   public:
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      GetXAttrMsg(EntryInfo* entryInfo, const std::string& name, int size)
         : BaseType(NETMSGTYPE_GetXAttr),
           entryInfoPtr(entryInfo), size(size), name(name)
      {
      }

      /**
       * For deserialization only!
       */
      GetXAttrMsg() : BaseType(NETMSGTYPE_GetXAttr) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % obj->name
            % obj->size;
      }

      bool supportsMirroring() const { return true; }

   private:

      // for serialization
      EntryInfo* entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;

      int32_t size;
      std::string name;

   public:
      // getters and setters

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

      const std::string& getName(void) const
      {
         return this->name;
      }

      int getSize(void) const
      {
         return this->size;
      }
};

#endif /*LISTXATTRMSG_H_*/
