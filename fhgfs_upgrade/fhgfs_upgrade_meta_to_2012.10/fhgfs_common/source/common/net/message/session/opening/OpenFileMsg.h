#ifndef OPENFILEMSG_H_
#define OPENFILEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/storage/Path.h>


class OpenFileMsg : public NetMessage
{
   public:

      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       * @param enrtyInfo just a reference, so do not free it as long as you use this object!
       * @param accessFlags OPENFILE_ACCESS_... flags
       */
      OpenFileMsg(const char* sessionID, EntryInfo* entryInfo, unsigned accessFlags) :
         NetMessage(NETMSGTYPE_OpenFile)
      {
         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);

         this->entryInfoPtr = entryInfo;

         this->accessFlags = accessFlags;
      }


   protected:
      /**
       * For deserialization only!
       */
      OpenFileMsg() : NetMessage(NETMSGTYPE_OpenFile) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH                        +
            Serialization::serialLenUInt()                  + // accessFlags
            Serialization::serialLenStrAlign4(sessionIDLen) + // sessionID
            this->entryInfoPtr->serialLen();                  // entryInfo
      }


   private:
      const char* sessionID;
      unsigned sessionIDLen;

      unsigned accessFlags;

      // serialization
      EntryInfo* entryInfoPtr;

      // deserialization
      EntryInfo entryInfo;

   public:

      // getters & setters
      const char* getSessionID() const
      {
         return sessionID;
      }

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }

      unsigned getAccessFlags() const
      {
         return accessFlags;
      }

};

#endif /*OPENFILEMSG_H_*/
