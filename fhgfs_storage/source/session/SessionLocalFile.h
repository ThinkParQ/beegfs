#ifndef SESSIONLOCALFILE_H_
#define SESSIONLOCALFILE_H_

#include <common/nodes/Node.h>
#include <common/storage/Path.h>
#include <common/storage/PathInfo.h>
#include <common/storage/quota/QuotaData.h>
#include <common/threading/Mutex.h>
#include <common/threading/SafeMutexLock.h>
#include <common/Common.h>
#include <nodes/NodeStoreServersEx.h>


/**
 * Represents the client session information for an open chunk file.
 */
class SessionLocalFile
{
   public:
      /**
       * @param fileHandleID format: <ownerID>#<fileID>
       * @param openFlags system flags for open()
       * @param serverCrashed true if session was created after a server crash, mark session as
       * dirty
       */
      SessionLocalFile(std::string fileHandleID, uint16_t targetID, std::string fileID,
         int openFlags, bool serverCrashed) : fileHandleID(fileHandleID), targetID(targetID),
         fileID(fileID)
      {
         this->fileDescriptor = -1; // initialize as invalid file descriptor
         this->openFlags = openFlags;
         this->offset = -1; // initialize as invalid offset (will be set on file open)

         this->isMirrorSession = false;

         this->removeOnRelease = false;

         this->writeCounter = 0;
         this->readCounter = 0;
         this->lastReadAheadTrigger = 0;

         this->serverCrashed = serverCrashed;
      }

      /**
       * For dezerialisation only
       */
      SessionLocalFile()
      {
         this->fileDescriptor = -1; // initialize as invalid file descriptor
         this->offset = -1; // initialize as invalid offset (will be set on file open)
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->removeOnRelease
            % obj->fileHandleID
            % obj->targetID
            % obj->fileID
            % obj->openFlags
            % obj->offset
            % obj->isMirrorSession
            % obj->serverCrashed;

         serializeNodeID(obj, ctx);

         ctx
            % serdes::atomicAs<int64_t>(obj->writeCounter)
            % serdes::atomicAs<int64_t>(obj->readCounter)
            % serdes::atomicAs<int64_t>(obj->lastReadAheadTrigger);
      }

      FhgfsOpsErr openFile(int targetFD, PathInfo *pathInfo, bool isWriteOpen,
         SessionQuotaInfo* quotaInfo);

      NodeHandle setMirrorNodeExclusive(NodeHandle mirrorNode);

   private:
      static void serializeNodeID(const SessionLocalFile* obj, Serializer& ser)
      {
         if (!obj->mirrorNode)
            ser % uint16_t(0);
         else
            ser % uint16_t(obj->mirrorNode->getNumID().val());
      }

      static void serializeNodeID(SessionLocalFile* obj, Deserializer& des);

   private:
      bool removeOnRelease; // remove on last ref drop (for internal use by the sessionstore only!)

      std::string fileHandleID;
      uint16_t targetID;
      std::string fileID;
      int32_t openFlags; // system flags for open()

      int fileDescriptor; // file descriptor -1 for invalid (=not opened yet)
      int64_t offset; // negative value for unspecified/invalid offset

      NodeHandle mirrorNode; // the node to which all writes should be mirrored
      bool isMirrorSession; // true if this is the mirror session of a file

      AtomicInt64 writeCounter; // how much sequential data we have written after open/sync_file_range
      AtomicInt64 readCounter; // how much sequential data we have read since open / last seek
      AtomicInt64 lastReadAheadTrigger; // last readCounter value at which read-ahead was triggered

      Mutex sessionMutex;

      bool serverCrashed; // true if session was created after a server crash, mark session as dirty

   public:

      // getters & setters
      bool getRemoveOnRelease()
      {
         return removeOnRelease;
      }

      void setRemoveOnRelease(bool removeOnRelease)
      {
         this->removeOnRelease = removeOnRelease;
      }

      std::string getFileHandleID() const
      {
         return fileHandleID;
      }

      uint16_t getTargetID() const
      {
         return targetID;
      }

      std::string getFileID() const
      {
         return fileID;
      }

      int getFD()
      {
         if (this->fileDescriptor != -1) // optimization: try without a lock first
            return this->fileDescriptor;

         SafeMutexLock safeMutex(&this->sessionMutex); // L O C K

         int fd = this->fileDescriptor;

         safeMutex.unlock(); // U N L O C K

         return fd;
      }
      
      void setFDUnlocked(int fd)
      {
         this->fileDescriptor = fd;
      }

      int getOpenFlags() const
      {
         return openFlags;
      }

      bool getIsDirectIO() const
      {
         return (this->openFlags & (O_DIRECT | O_SYNC) ) != 0;
      }

      int64_t getOffset()
      {
         SafeMutexLock safeMutex(&this->sessionMutex); // L O C K

         int64_t offset = this->offset;

         safeMutex.unlock(); // U N L O C K

         return offset;
      }

      void setOffset(int64_t offset)
      {
         SafeMutexLock safeMutex(&this->sessionMutex); // L O C K

         setOffsetUnlocked(offset);

         safeMutex.unlock(); // U N L O C K
      }

      void setOffsetUnlocked(int64_t offset)
      {
         this->offset = offset;
      }

      NodeHandle getMirrorNode() const
      {
         return mirrorNode;
      }

      bool getIsMirrorSession() const
      {
         return isMirrorSession;
      }

      void setIsMirrorSession(bool isMirrorSession)
      {
         this->isMirrorSession = isMirrorSession;
      }

      void resetWriteCounter()
      {
         this->writeCounter = 0;
      }

      void incWriteCounter(int64_t size)
      {
         this->writeCounter.increase(size);
      }

      int64_t getWriteCounter()
      {
         return this->writeCounter.read();
      }

      int64_t getReadCounter()
      {
         return this->readCounter.read();
      }

      void resetReadCounter()
      {
         this->readCounter.setZero();
      }

      void incReadCounter(int64_t size)
      {
         this->readCounter.increase(size);
      }

      int64_t getLastReadAheadTrigger()
      {
         return this->lastReadAheadTrigger.read();
      }

      void resetLastReadAheadTrigger()
      {
         this->lastReadAheadTrigger.setZero();
      }

      void setLastReadAheadTrigger(int64_t lastReadAheadTrigger)
      {
         this->lastReadAheadTrigger.set(lastReadAheadTrigger);
      }

      bool isServerCrashed()
      {
         return this->serverCrashed;
      }
};

#endif /*SESSIONLOCALFILE_H_*/
