#ifndef CLOSECHUNKFILEMSG_H_
#define CLOSECHUNKFILEMSG_H_

#include <common/net/message/NetMessage.h>


#define CLOSECHUNKFILEMSG_FLAG_NODYNAMICATTRIBS    1 /* skip retrieval of current dyn attribs */
#define CLOSECHUNKFILEMSG_FLAG_ISMIRROR            2 /* if this is a mirror session */


class CloseChunkFileMsg : public NetMessage
{
   public:

      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      CloseChunkFileMsg(std::string& sessionID, std::string& fileHandleID, uint16_t targetID) :
         NetMessage(NETMSGTYPE_CloseChunkFile)
      {
         this->sessionID = sessionID.c_str();
         this->sessionIDLen = sessionID.length();

         this->fileHandleID = fileHandleID.c_str();
         this->fileHandleIDLen = fileHandleID.length();

         this->targetID = targetID;

         this->flags = 0;
      }


   protected:
      /**
       * For deserialization only!
       */
      CloseChunkFileMsg() : NetMessage(NETMSGTYPE_CloseChunkFile) { }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStrAlign4(sessionIDLen) +
            Serialization::serialLenStrAlign4(fileHandleIDLen) +
            Serialization::serialLenUInt() + // flags
            Serialization::serialLenUShort(); // targetID
      }


   private:
      unsigned sessionIDLen;
      const char* sessionID;
      unsigned fileHandleIDLen;
      const char* fileHandleID;
      uint16_t targetID;
      unsigned flags; // CLOSECHUNKFILEMSG_FLAG_...


   public:
      // getters & setters

      const char* getSessionID() const
      {
         return sessionID;
      }
      
      const char* getFileHandleID() const
      {
         return fileHandleID;
      }

      uint16_t getTargetID() const
      {
         return targetID;
      }

      unsigned getFlags() const
      {
         return flags;
      }

      void setFlags(unsigned flags)
      {
         this->flags = flags;
      }

      // inliners

      /**
       * Test flag. (For convenience and readability.)
       *
       * @return true if given flag is set.
       */
      bool isFlagSet(unsigned flag) const
      {
         return (this->flags & flag) != 0;
      }

};


#endif /*CLOSECHUNKFILEMSG_H_*/
