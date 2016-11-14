#ifndef GETENTRYINFOMSG_H_
#define GETENTRYINFOMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>


class GetEntryInfoMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param path just a reference, so do not free it as long as you use this object!
       */
      GetEntryInfoMsg(EntryInfo* entryInfo) : NetMessage(NETMSGTYPE_GetEntryInfo)
      {
         this->entryInfoPtr = entryInfo;
      }


   protected:
      GetEntryInfoMsg() : NetMessage(NETMSGTYPE_GetEntryInfo)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH + this->entryInfoPtr->serialLen();
      }


   private:
      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object!

      // for deserialization
      EntryInfo entryInfo;


   public:

      // inliners

      // getters & setters

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

};


#endif /*GETENTRYINFOMSG_H_*/
