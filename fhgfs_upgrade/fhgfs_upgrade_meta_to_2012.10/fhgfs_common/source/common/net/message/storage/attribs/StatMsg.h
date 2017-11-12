#ifndef STATMSG_H_
#define STATMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>
#include <common/storage/EntryInfo.h>


class StatMsg : public NetMessage
{
   friend class AbstractNetMessageFactory;
   
   public:
      
      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      StatMsg(EntryInfo* entryInfo) : NetMessage(NETMSGTYPE_Stat)
      {
         this->entryInfoPtr = entryInfo;
      }


   protected:
      /**
       * For deserialization only!
       */
      StatMsg() : NetMessage(NETMSGTYPE_Stat) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            this->entryInfoPtr->serialLen();
      }


   private:

      // for serialization
      EntryInfo* entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;
   public:
   
      // getters & setters

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }
};

#endif /*STATMSG_H_*/
