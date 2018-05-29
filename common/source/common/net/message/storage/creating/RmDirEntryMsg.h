#ifndef RMDIRENTRYMSG_H_
#define RMDIRENTRYMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/EntryInfo.h>


class RmDirEntryMsg : public NetMessageSerdes<RmDirEntryMsg>
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param path just a reference, so do not free it as long as you use this object!
       */
      RmDirEntryMsg(EntryInfo* parentInfo, std::string& entryName) :
         BaseType(NETMSGTYPE_RmDirEntry), entryName(entryName)
      {
         this->parentInfoPtr = parentInfo;

      }

      RmDirEntryMsg() : BaseType(NETMSGTYPE_RmDirEntry)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->parentInfoPtr, obj->parentInfo)
            % serdes::stringAlign4(obj->entryName);
      }

   private:

      std::string entryName;

      // for serialization
      EntryInfo* parentInfoPtr;

      // for deserialization
      EntryInfo parentInfo;



   public:

      std::string getEntryName() const
      {
         return this->entryName;
      }

      EntryInfo* getParentInfo()
      {
         return &this->parentInfo;
      }

      // getters & setters

};


#endif /* RMDIRENTRYMSG_H_ */
