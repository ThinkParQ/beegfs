#ifndef FLOCKRANGEMSG_H_
#define FLOCKRANGEMSG_H_

#include <common/net/message/NetMessage.h>


class FLockRangeMsg : public NetMessage
{
   public:

      /**
       * @param clientID just a reference, so do not free it as long as you use this object!
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param lockTypeFlags ENTRYLOCKTYPE_... flags
       * @param lockAckID just a reference, so do not free it as long as you use this object!
       */
      FLockRangeMsg(const char* clientID, const char* fileHandleID, int ownerPID, int lockTypeFlags,
         int64_t start, int64_t end, const char* lockAckID) : NetMessage(NETMSGTYPE_FLockRange)
      {
         this->clientID = clientID;
         this->clientIDLen = strlen(clientID);

         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->ownerPID = ownerPID;
         this->lockTypeFlags = lockTypeFlags;

         this->start = start;
         this->end = end;

         this->lockAckID = lockAckID;
         this->lockAckIDLen = strlen(lockAckID);
      }


   protected:
      /**
       * For deserialization only!
       */
      FLockRangeMsg() : NetMessage(NETMSGTYPE_FLockRange) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH         +
            Serialization::serialLenUInt64() + // start
            Serialization::serialLenUInt64() + // end
            Serialization::serialLenInt()    + // ownerPID
            Serialization::serialLenInt()    + // lockTypeFlags
            entryInfoPtr->serialLen()        + // entryInfo
            Serialization::serialLenStrAlign4(clientIDLen)     + // clientID
            Serialization::serialLenStrAlign4(fileHandleIDLen) + // fileHandleID
            Serialization::serialLenStrAlign4(lockAckIDLen);     // lockAckID
      }


   private:
      const char* clientID;
      unsigned clientIDLen;
      const char* fileHandleID;
      unsigned fileHandleIDLen;
      int ownerPID; // pid on client (just informative, because shared on fork() )
      int lockTypeFlags; // ENTRYLOCKTYPE_...
      uint64_t start;
      uint64_t end;
      const char* lockAckID; // ID for ack message when log is granted
      unsigned lockAckIDLen;

      // for serialization
      EntryInfo* entryInfoPtr;

      // for deserialization
      EntryInfo entryInfo;

   public:
      // getters & setters
      const char* getClientID() const
      {
         return clientID;
      }

      const char* getFileHandleID() const
      {
         return fileHandleID;
      }

      int getOwnerPID() const
      {
         return ownerPID;
      }

      int getLockTypeFlags() const
      {
         return lockTypeFlags;
      }

      uint64_t getStart() const
      {
         return start;
      }

      uint64_t getEnd() const
      {
         return end;
      }

      const char* getLockAckID() const
      {
         return lockAckID;
      }

      EntryInfo* getEntryInfo(void)
      {
         return &this->entryInfo;
      }
};

#endif /* FLOCKRANGEMSG_H_ */
