#ifndef META_SYNCSLAVEBASE_H
#define META_SYNCSLAVEBASE_H

#include <common/net/sock/Socket.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/PThread.h>
#include <app/App.h>

class DirEntry;

class SyncSlaveBase : public PThread
{
   public:
      bool getIsRunning()
      {
         std::lock_guard<Mutex> lock(stateMutex);
         return this->isRunning;
      }

      void setOnlyTerminateIfIdle(bool value)
      {
         onlyTerminateIfIdle.set(value);
      }

      bool getOnlyTerminateIfIdle()
      {
         return onlyTerminateIfIdle.read();
      }

   protected:
      BuddyResyncJob* parentJob;

      NumNodeID buddyNodeID;

      Mutex stateMutex;
      Condition isRunningChangeCond;

      AtomicSizeT onlyTerminateIfIdle;

      bool isRunning;

      Path basePath;

      SyncSlaveBase(const std::string& threadName, BuddyResyncJob& parentJob,
            const NumNodeID buddyNodeID):
         PThread(threadName), parentJob(&parentJob), buddyNodeID(buddyNodeID), isRunning(false)
      {
      }

      virtual void run() override;
      virtual void syncLoop() = 0;

      FhgfsOpsErr resyncAt(const Path& basePath, bool wholeDirectory,
         FhgfsOpsErr (*streamFn)(Socket*, void*), void* context);

      FhgfsOpsErr streamDentry(Socket& socket, const Path& contDirRelPath, const std::string& name);
      FhgfsOpsErr streamInode(Socket& socket, const Path& inodeRelPath, const bool isDirectory);

      FhgfsOpsErr deleteDentry(Socket& socket, const Path& contDirRelPath, const std::string& name);
      FhgfsOpsErr deleteInode(Socket& socket, const Path& inodeRelPath, const bool isDirectory);

      void setIsRunning(bool isRunning)
      {
         std::lock_guard<Mutex> lock(stateMutex);
         this->isRunning = isRunning;
         isRunningChangeCond.broadcast();
      }

      bool getSelfTerminateNotIdle()
      {
         return getSelfTerminate() && !getOnlyTerminateIfIdle();
      }

      template<typename ValueT>
      static FhgfsOpsErr sendResyncPacket(Socket& socket, const ValueT& value)
      {
         Serializer ser;
         ser % value;

         const unsigned packetSize = ser.size();
         const unsigned totalSize = packetSize + sizeof(uint32_t);

         const std::tuple<uint32_t, const ValueT&> packet(packetSize, value);

         std::unique_ptr<char[]> buffer(new (std::nothrow) char[totalSize]);
         if (!buffer)
         {
            LOG(MIRRORING, ERR, "Could not allocate memory for resync packet.");
            return FhgfsOpsErr_OUTOFMEM;
         }

         ser = {buffer.get(), totalSize};
         ser % packet;
         if (!ser.good())
         {
            LOG(MIRRORING, ERR, "Serialization of resync packet failed.");
            return FhgfsOpsErr_INTERNAL;
         }

         socket.send(buffer.get(), totalSize, 0);
         return FhgfsOpsErr_SUCCESS;
      }

      static FhgfsOpsErr receiveAck(Socket& socket);

   private:
      typedef std::tuple<
            MetaSyncFileType,
            const std::string&, // relative path
            bool,               // is hardlink?
            const std::string&, // link target entry id
            bool                // is deletion?
         > LinkDentryInfo;

      typedef std::tuple<
            MetaSyncFileType,
            const std::string&,       // relative path
            bool,                     // is hardlink?
            const std::vector<char>&, // dentry raw content
            bool                      // is deletion?
         > FullDentryInfo;

      typedef std::tuple<
            MetaSyncFileType,
            const std::string&,       // relative path
            const std::vector<char>&, // inode raw content
            bool                      // is deletion?
         > InodeInfo;
};

#endif
