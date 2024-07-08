#ifndef FLOCKAPPENDMSG_H_
#define FLOCKAPPENDMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/EntryInfo.h>

class FLockAppendMsg : public MirroredMessageBase<FLockAppendMsg>
{
   public:

      /**
       * @param clientID
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param lockTypeFlags ENTRYLOCKTYPE_... flags
       * @param lockAckID just a reference, so do not free it as long as you use this object!
       */
      FLockAppendMsg(const NumNodeID clientNumID, const char* fileHandleID, const int64_t clientFD,
         const int ownerPID, const int lockTypeFlags, const char* lockAckID) :
         BaseType(NETMSGTYPE_FLockAppend)
      {
         this->clientNumID = clientNumID;

         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->clientFD = clientFD;
         this->ownerPID = ownerPID;
         this->lockTypeFlags = lockTypeFlags;

         this->lockAckID = lockAckID;
         this->lockAckIDLen = strlen(lockAckID);
      }

      /**
       * For deserialization only!
       */
      FLockAppendMsg() : BaseType(NETMSGTYPE_FLockAppend) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->clientNumID
            % obj->clientFD
            % obj->ownerPID
            % obj->lockTypeFlags
            % serdes::backedPtr(obj->entryInfoPtr, obj->entryInfo)
            % serdes::rawString(obj->fileHandleID, obj->fileHandleIDLen, 4)
            % serdes::rawString(obj->lockAckID, obj->lockAckIDLen, 4);
      }

      bool supportsMirroring() const { return true; }

   private:
      NumNodeID clientNumID;
      const char* fileHandleID;
      unsigned fileHandleIDLen;
      int64_t clientFD; /* some client-wide unique file handle
                         * corresponds to 'struct file_lock* fileLock->fl_file' on the kernel
                         * client side.
                         */
      int32_t ownerPID; // pid on client (just informative, because shared on fork() )
      int32_t lockTypeFlags; // ENTRYLOCKTYPE_...
      const char* lockAckID; // ID for ack message when log is granted
      unsigned lockAckIDLen;

      // for serialization
      EntryInfo* entryInfoPtr; // not owned by this object

      // for deserialization
      EntryInfo entryInfo;


   public:
      // getters & setters
      NumNodeID getClientNumID() const
      {
         return clientNumID;
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

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }
};

#endif /* FLOCKAPPENDMSG_H_ */
