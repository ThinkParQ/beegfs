#ifndef WRITELOCALFILEMSG_H_
#define WRITELOCALFILEMSG_H_

#include <common/net/message/NetMessage.h>

class WriteLocalFileMsg : public NetMessage
{
   public:
      
      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       */
      WriteLocalFileMsg(const char* sessionID, const char* fileHandleID, uint16_t targetID,
         unsigned accessFlags, int64_t offset, int64_t count) :
         NetMessage(NETMSGTYPE_WriteLocalFile)
      {
         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);
         
         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->targetID = targetID;
         this->mirrorToTargetID = 0;
         this->mirroredFromTargetID = 0;

         this->accessFlags = accessFlags;

         this->offset = offset;
         this->count = count;

         this->disableIO = false;
      }


     
   protected:
      /**
       * For deserialization only!
       */
      WriteLocalFileMsg() : NetMessage(NETMSGTYPE_WriteLocalFile) {}


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
            Serialization::serialLenUShort() + // mirrorToTargetID
            Serialization::serialLenUShort() + // mirroredFromTargetID
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
      uint16_t mirrorToTargetID; // the data in this msg shall be forwarded to this mirror
      uint16_t mirroredFromTargetID; // this msg was forwarded from this original target
      bool disableIO; // true to disable the actual write() syscalls => network benchmarking
      

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

      uint16_t getMirrorToTargetID() const
      {
         return mirrorToTargetID;
      }

      void setMirrorToTargetID(uint16_t mirrorToTargetID)
      {
         this->mirrorToTargetID = mirrorToTargetID;
      }

      uint16_t getMirroredFromTargetID() const
      {
         return mirroredFromTargetID;
      }

      void setMirroredFromTargetID(uint16_t mirroredFromTargetID)
      {
         this->mirroredFromTargetID = mirroredFromTargetID;
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


      // inliners

      void sendData(const char* data, Socket* sock)
      {
         sock->send(data, count, 0);
      }

};

#endif /*WRITELOCALFILEMSG_H_*/
