#ifndef FLOCKRANGEMSG_H_
#define FLOCKRANGEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/EntryInfo.h>


class FLockRangeMsg : public MirroredMessageBase<FLockRangeMsg>
{
   public:

      /**
       * @param clientNumID
       * @param fileHandleID just a reference, so do not free it as long as you use this object!
       * @param lockTypeFlags ENTRYLOCKTYPE_... flags
       * @param lockAckID just a reference, so do not free it as long as you use this object!
       */
      FLockRangeMsg(const NumNodeID clientNumID, const char* fileHandleID, const int ownerPID,
         const int lockTypeFlags, const int64_t start, const int64_t end, const char* lockAckID) :
         BaseType(NETMSGTYPE_FLockRange)
      {
         this->clientNumID = clientNumID;

         this->fileHandleID = fileHandleID;
         this->fileHandleIDLen = strlen(fileHandleID);

         this->ownerPID = ownerPID;
         this->lockTypeFlags = lockTypeFlags;

         this->start = start;
         this->end = end;

         this->lockAckID = lockAckID;
         this->lockAckIDLen = strlen(lockAckID);
      }

      /**
       * For deserialization only!
       */
      FLockRangeMsg() : BaseType(NETMSGTYPE_FLockRange) {}

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->clientNumID
            % obj->start
            % obj->end
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
      int32_t ownerPID; // pid on client (just informative, because shared on fork() )
      int32_t lockTypeFlags; // ENTRYLOCKTYPE_...
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
      NumNodeID getClientNumID() const
      {
         return clientNumID;
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

      EntryInfo* getEntryInfo()
      {
         return &this->entryInfo;
      }
};

#endif /* FLOCKRANGEMSG_H_ */
