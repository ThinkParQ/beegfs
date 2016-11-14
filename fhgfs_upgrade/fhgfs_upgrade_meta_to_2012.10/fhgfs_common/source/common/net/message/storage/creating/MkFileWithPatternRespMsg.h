#ifndef MKFILEWITHPATTERNRESPMSG_H_
#define MKFILEWITHPATTERNRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class MkFileWithPatternRespMsg : public NetMessage
{
   public:

      /**
       * @param entryInfo just a reference, so do not free it as long as you use this object!
       */
      MkFileWithPatternRespMsg(int result, EntryInfo* entryInfo) :
         NetMessage(NETMSGTYPE_MkFileWithPatternResp)
      {
         this->result = result;
         this->entryInfoPtr = entryInfo;
      }

      MkFileWithPatternRespMsg() : NetMessage(NETMSGTYPE_MkFileWithPatternResp)
      {
      }

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt() + // result
            this->entryInfoPtr->serialLen(); // entryInfo
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
         return this->entryInfoPtr;
      }

};

#endif /* MKFILEWITHPATTERNRESPMSG_H_ */
