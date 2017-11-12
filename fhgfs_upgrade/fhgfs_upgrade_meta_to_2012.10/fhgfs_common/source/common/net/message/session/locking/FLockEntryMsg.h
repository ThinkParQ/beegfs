#ifndef FLOCKENTRYMSG_H_
#define FLOCKENTRYMSG_H_

#include <common/net/message/NetMessage.h>


class FLockEntryMsg : public NetMessage
{
   public:

      /**
       * @param clientID just a reference, so do not free it as long as you use this object!
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param lockTypeFlags ENTRYLOCKTYPE_... flags
       * @param lockAckID just a reference, so do not free it as long as you use this object!
       */
      FLockEntryMsg(const char* clientID, const char* fileHandleID, int64_t clientFD, int ownerPID,
         int lockTypeFlags, const char* lockAckID) : NetMessage(NETMSGTYPE_FLockEntry)
      {
         this->clientID = clientID;
         this->clientIDLen = strlen(clientID);

         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->clientFD = clientFD;
         this->ownerPID = ownerPID;
         this->lockTypeFlags = lockTypeFlags;

         this->lockAckID = lockAckID;
         this->lockAckIDLen = strlen(lockAckID);
      }


   protected:
      /**
       * For deserialization only!
       */
      FLockEntryMsg() : NetMessage(NETMSGTYPE_FLockEntry) {}


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt64() + // clientFD
            Serialization::serialLenInt() + // ownerPID
            Serialization::serialLenInt() + // lockTypeFlags
            this->entryInfoPtr->serialLen()  + // entryInfo
            Serialization::serialLenStrAlign4(clientIDLen)     + // clientID
            Serialization::serialLenStrAlign4(fileHandleIDLen) + // fileHandleID
            Serialization::serialLenStrAlign4(lockAckIDLen);     // lockAckID
      }


   private:
      const char* clientID;
      unsigned clientIDLen;
      const char* fileHandleID;
      unsigned fileHandleIDLen;
      int64_t clientFD; /* some client-wide unique file handle
                         * corresponds to 'struct file_lock* fileLock->fl_file' on the kernel
                         * client side.
                         */
      int ownerPID; // pid on client (just informative, because shared on fork() )
      int lockTypeFlags; // ENTRYLOCKTYPE_...
      const char* lockAckID; // ID for ack message when log is granted
      unsigned lockAckIDLen;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object

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

      int64_t getClientFD() const
      {
         return clientFD;
      }

      int getOwnerPID() const
      {
         return ownerPID;
      }

      int getLockTypeFlags() const
      {
         return lockTypeFlags;
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

#endif /* FLOCKENTRYMSG_H_ */
