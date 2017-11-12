#ifndef MKDIRRESPMSG_H_
#define MKDIRRESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class MkDirRespMsg : public NetMessage
{
   public:

      /**
       * @param ownerNodeID just a reference, so do not free it as long as you use this object!
       * @param entryID just a reference, so do not free it as long as you use this object!
       */
      MkDirRespMsg(int result, EntryInfo* entryInfo) :
         NetMessage(NETMSGTYPE_MkDirResp)
      {
         this->result       = result;
         this->entryInfoPtr = entryInfo;
      }

      MkDirRespMsg() : NetMessage(NETMSGTYPE_MkDirResp)
      {
      }

   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH         +
            Serialization::serialLenInt()    + // result
            this->entryInfoPtr->serialLen();   // entryInfo
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

#endif /*MKDIRRESPMSG_H_*/
