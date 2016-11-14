#ifndef REFRESHENTRYINFOMSG_H_
#define REFRESHENTRYINFOMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/EntryInfo.h>


class RefreshEntryInfoMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;

   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      RefreshEntryInfoMsg(EntryInfo* entryInfo) : NetMessage(NETMSGTYPE_RefreshEntryInfo)
      {
         this->entryInfoPtr = entryInfo;
      }


   protected:

      RefreshEntryInfoMsg() : NetMessage(NETMSGTYPE_RefreshEntryInfo)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            this->entryInfoPtr->serialLen();
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


#endif /* REFRESHENTRYINFOMSG_H_ */
