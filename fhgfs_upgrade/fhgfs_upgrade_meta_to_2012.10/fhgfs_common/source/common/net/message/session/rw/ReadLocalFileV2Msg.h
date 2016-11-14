#ifndef READLOCALFILEV2MSG_H_
#define READLOCALFILEV2MSG_H_

#include <common/net/message/NetMessage.h>

class ReadLocalFileV2Msg : public NetMessage
{
   public:
      
      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      ReadLocalFileV2Msg(const char* sessionID, const char* fileHandleID, uint16_t targetID,
         unsigned accessFlags, int64_t offset, int64_t count) :
         NetMessage(NETMSGTYPE_ReadLocalFileV2)
      {
         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);
         
         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->targetID = targetID;

         this->accessFlags = accessFlags;

         this->offset = offset;
         this->count = count;

         this->disableIO = false;
      }


     
   protected:
      /**
       * For deserialization only!
       */
      ReadLocalFileV2Msg() : NetMessage(NETMSGTYPE_ReadLocalFileV2) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt64() + // offset
            Serialization::serialLenInt64() + // count
            Serialization::serialLenUInt() + // accessFlags
            Serialization::serialLenStrAlign4(fileHandleIDLen) +
            Serialization::serialLenStrAlign4(sessionIDLen) +
            Serialization::serialLenUShort() + // targetID
            Serialization::serialLenBool(); // disableIO
      }


   private:
      int64_t offset;
      int64_t count;
      unsigned accessFlags;
      const char* fileHandleID;
      unsigned fileHandleIDLen;
      const char* sessionID;
      unsigned sessionIDLen;
      uint16_t targetID;
      bool disableIO; // true to disable the actual read() syscalls => network benchmarking

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

      unsigned getAccessFlags() const
      {
         return accessFlags;
      }
      
      int64_t getOffset() const
      {
         return offset;
      }
      
      int64_t getCount() const
      {
         return count;
      }
      
      bool getDisableIO() const
      {
         return disableIO;
      }

      void setDisableIO(bool disableIO)
      {
         this->disableIO = disableIO;
      }
};


#endif /*READLOCALFILEV2MSG_H_*/
