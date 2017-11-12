#ifndef RMLOCALDIRMSG_H_
#define RMLOCALDIRMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>

class RmLocalDirMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param delEntryInfo just a reference, so do not free it as long as you use this object!
       */
      RmLocalDirMsg(EntryInfo* delEntryInfo) :
         NetMessage(NETMSGTYPE_RmLocalDir)
      {
         this->delEntryInfoPtr = delEntryInfo;

      }


   protected:
      RmLocalDirMsg() : NetMessage(NETMSGTYPE_RmLocalDir)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            this->delEntryInfoPtr->serialLen(); // delEntryInfo
      }


   private:

      // for serialization
      EntryInfo* delEntryInfoPtr;

      // for deserialization
      EntryInfo delEntryInfo;

   public:
   
      // inliners   

      // getters & setters
      EntryInfo* getDelEntryInfo(void)
      {
         return &this->delEntryInfo;
      }

};

#endif /*RMLOCALDIRMSG_H_*/
