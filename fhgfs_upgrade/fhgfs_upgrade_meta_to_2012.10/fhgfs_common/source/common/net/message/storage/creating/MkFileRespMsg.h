#ifndef MKFILERESPMSG_H_
#define MKFILERESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class MkFileRespMsg : public NetMessage
{
   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkFileRespMsg(int result, EntryInfo* entryInfo) :
         NetMessage(NETMSGTYPE_MkFileResp)
      {
         this->result = result;
         this->entryInfoPtr = entryInfo;
      }

      MkFileRespMsg() : NetMessage(NETMSGTYPE_MkFileResp)
      {
      }

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // result
            this->entryInfoPtr->serialLen();
      }


   private:
      int result;

      // for serialization
      EntryInfo* entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;

   public:

      // inliners

      // getters & setters
      int getResult()
      {
         return result;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }

};

#endif /*MKFILERESPMSG_H_*/
