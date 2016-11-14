#ifndef FSYNCLOCALFILEMSG_H_
#define FSYNCLOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>


#define FSYNCLOCALFILEMSG_FLAG_ISMIRROR      1 /* if this is a mirror session */


class FSyncLocalFileMsg : public NetMessage
{
   public:
      
      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      FSyncLocalFileMsg(const char* sessionID, const char* fileHandleID, uint16_t targetID) :
         NetMessage(NETMSGTYPE_FSyncLocalFile)
      {
         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);
         
         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->targetID = targetID;

         this->flags = 0;
      }


   protected:
      /**
       * For deserialization only!
       */
      FSyncLocalFileMsg() : NetMessage(NETMSGTYPE_FSyncLocalFile) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUShort() + // flags
            Serialization::serialLenUShort() + // targetID
            Serialization::serialLenStrAlign4(sessionIDLen) +
            Serialization::serialLenStrAlign4(fileHandleIDLen);
      }


   private:
      const char* sessionID;
      unsigned sessionIDLen;
      const char* fileHandleID;
      unsigned fileHandleIDLen;
      uint16_t targetID;
      uint16_t flags; // FSYNCLOCALFILEMSG_FLAG_...
      

   public:
      // getters & setters
      const char* getSessionID()
      {
         return sessionID;
      }
      
      const char* getFileHandleID()
      {
         return fileHandleID;
      }
      
      uint16_t getTargetID()
      {
         return targetID;
      }

      uint16_t getFlags() const
      {
         return flags;
      }

      void setFlags(uint16_t flags)
      {
         this->flags = flags;
      }

      // inliners

      /**
       * Test flag. (For convenience and readability.)
       *
       * @return true if given flag is set.
       */
      bool isFlagSet(uint16_t flag) const
      {
         return (this->flags & flag) != 0;
      }
};


#endif /*FSYNCLOCALFILEMSG_H_*/
