#ifndef SESSIONLOCALFILE_H_
#define SESSIONLOCALFILE_H_

#include <common/Common.h>
#include <common/nodes/Node.h>
#include <common/storage/Path.h>
#include <common/storage/PathInfo.h>
#include <common/storage/quota/QuotaData.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/FDHandle.h>

#include <atomic>

/**
 * Represents the client session information for an open chunk file.
 */
class SessionLocalFile
{
   public:
      class Handle
      {
         friend class SessionLocalFile;

         public:
            Handle() = default;

            Handle(const std::string& id, FDHandle fd):
               id(id), fd(std::move(fd)), claimed(0)
            {
            }

            bool close();

            const FDHandle& getFD() const { return fd; }
            const std::string& getID() const { return id; }

         private:
            std::string id;
            FDHandle fd;
            // for use by SessionLocalFile::releaseLastReference. only one caller may receive the
            // handle if multiple threads try to release the last reference concurrently. we could
            // also do this under a lock in SessionLocalFileStore but don't since we don't expect
            // contention on the release path.
            std::atomic<bool> claimed;
      };

   public:
      /**
       * @param fileHandleID format: <ownerID>#<fileID>
       * @param openFlags system flags for open()
       * @param serverCrashed true if session was created after a server crash, mark session as
       * dirty
       */
      SessionLocalFile(const std::string& fileHandleID, uint16_t targetID, std::string fileID,
            int openFlags, bool serverCrashed) :
         handle(std::make_shared<Handle>(fileHandleID, FDHandle())), targetID(targetID),
         fileID(fileID)
      {
         this->openFlags = openFlags;
         this->offset = -1; // initialize as invalid offset (will be set on file open)

         this->isMirrorSession = false;

         this->writeCounter = 0;
         this->readCounter = 0;
         this->lastReadAheadTrigger = 0;

         this->serverCrashed = serverCrashed;
      }

      /**
       * For dezerialisation only
       */
      SessionLocalFile():
         handle(std::make_shared<Handle>())
      {
         this->offset = -1; // initialize as invalid offset (will be set on file open)
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->handle->id
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

      FhgfsOpsErr openFile(int targetFD, const PathInfo* pathInfo, bool isWriteOpen,
         const SessionQuotaInfo* quotaInfo);

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
      // holds information about the underlying filesystem state this session file refers to for
      // clients. this handle may be referenced by outsiders to separate removal of session files
      // from their respective stores and closing of underlying fs objects. the protocol for such
      // a close operation should be as follows:
      //  1. remove the session file from its store, retain a shared_ptr
      //  2. copy this handle to a temporary
      //  3. move the session file reference to a weak_ptr. if the weak_ptr is expired after the
      //     move, the handle may be closed
      //  4. claim the handle, and close() the handle if claiming succeeded
      std::shared_ptr<Handle> handle;

      uint16_t targetID;
      std::string fileID;
      int32_t openFlags; // system flags for open()

      int64_t offset; // negative value for unspecified/invalid offset

      NodeHandle mirrorNode; // the node to which all writes should be mirrored
      bool isMirrorSession; // true if this is the mirror session of a file

      AtomicInt64 writeCounter; // how much sequential data we have written after open/sync_file_range
      AtomicInt64 readCounter; // how much sequential data we have read since open / last seek
      AtomicInt64 lastReadAheadTrigger; // last readCounter value at which read-ahead was triggered

      Mutex sessionMutex;

      bool serverCrashed; // true if session was created after a server crash, mark session as dirty

      static std::shared_ptr<Handle> releaseLastReference(std::shared_ptr<SessionLocalFile> file)
      {
         auto handle = file->handle;
         std::weak_ptr<SessionLocalFile> weak(file);

         // see Handle::claimed for explanation
         file.reset();
         if (weak.expired() && !handle->claimed.exchange(true))
            return handle;
         else
            return nullptr;
      }

   public:
      bool close()
      {
         std::lock_guard<Mutex> lock(sessionMutex);

         return handle->close();
      }

      friend std::shared_ptr<Handle> releaseLastReference(std::shared_ptr<SessionLocalFile>&& file)
      {
         return SessionLocalFile::releaseLastReference(std::move(file));
      }

      std::string getFileHandleID() const
      {
         return handle->id;
      }

      uint16_t getTargetID() const
      {
         return targetID;
      }

      std::string getFileID() const
      {
         return fileID;
      }

      const FDHandle& getFD()
      {
         if (handle->fd.valid()) // optimization: try without a lock first
            return handle->fd;

         std::lock_guard<Mutex> const lock(sessionMutex);

         return handle->fd;
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
         std::lock_guard<Mutex> const lock(sessionMutex);

         return offset;
      }

      void setOffset(int64_t offset)
      {
         std::lock_guard<Mutex> const lock(sessionMutex);

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
