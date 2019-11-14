#ifndef FILEEVENTLOGGER_H_
#define FILEEVENTLOGGER_H_

#include <common/storage/FileEvent.h>
#include <common/threading/Atomics.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/Time.h>

#include <string>
#include <sys/socket.h>
#include <sys/un.h>

struct FileEventLogItem
{
   const static constexpr uint16_t formatVersionMajor = 1;
   const static constexpr uint16_t formatVersionMinor = 0;
   uint64_t droppedEventsSeq;
   uint64_t missedEventsSeq;

   struct FileEventData
   {
      FileEventType type;
      std::string entryId;
      std::string parentId;
      std::string path;
      std::string targetPath;
      std::string targetParentId;

      static void serialize(const FileEventData* obj, Serializer& ctx);
   };

   FileEventData event;

   static void serialize(const FileEventLogItem* obj, Serializer& ctx);
};

class FileEventLogger {
   public:
      FileEventLogger(const std::string& address);
      ~FileEventLogger();

      FileEventLogger(FileEventLogger&&) = delete;
      FileEventLogger& operator=(FileEventLogger&&) = delete;

      void log(const FileEvent& event, const std::string& entryId, const std::string& parentID,
               const std::string& targetParentID = "", bool missedAnyEvent=false);

   private:
      int sockFD;
      sockaddr_un targetAddr;

      uint64_t droppedEventsSeq;
      uint64_t missedEventsSeq;

      Mutex mtx;
      Time lastConnectionAttempt{true};

      bool tryReconnect();
};

#endif
