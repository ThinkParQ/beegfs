#ifndef SETATTRMSG_H_
#define SETATTRMSG_H_


#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/EntryInfo.h>


class SetAttrMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       * @param validAttribs a combination of SETATTR_CHANGE_...-Flags
       */
      SetAttrMsg(EntryInfo *entryInfo, int validAttribs, SettableFileAttribs* attribs) :
         NetMessage(NETMSGTYPE_SetAttr)
      {
         this->validAttribs = validAttribs;
         this->attribs = *attribs;
      }


   protected:
      /**
       * For deserialization only!
       */
      SetAttrMsg() : NetMessage(NETMSGTYPE_SetAttr) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // validAttribs
            Serialization::serialLenInt() + // mode
            Serialization::serialLenInt64() + // modificationTimeSecs
            Serialization::serialLenInt64() + // lastAccessTimeSecs
            Serialization::serialLenUInt() + // userID
            Serialization::serialLenUInt() + // groupID
            this->entryInfoPtr->serialLen(); // entryInfo
      }


   private:
      int validAttribs;
      SettableFileAttribs attribs;
   
      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;

   public:
   
      // getters & setters
      int getValidAttribs()
      {
         return validAttribs;
      }
      
      SettableFileAttribs* getAttribs()
      {
         return &attribs;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

};

#endif /*SETATTRMSG_H_*/
