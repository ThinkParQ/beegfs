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
   uint64_t droppedEventsSeq;
   uint64_t missedEventsSeq;
   FileEvent event;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->droppedEventsSeq
         % obj->missedEventsSeq
         % obj->event;
   }
};

class FileEventLogger {
   public:
      FileEventLogger(const std::string& address);
      ~FileEventLogger();

      FileEventLogger(FileEventLogger&&) = delete;
      FileEventLogger& operator=(FileEventLogger&&) = delete;

      void log(FileEvent event, bool missedAnyEvent);

   private:
      int sockFD;
      sockaddr_un targetAddr;

      uint64_t droppedEventsSeq;
      uint64_t missedEventsSeq;

      Mutex mtx;
      Time lastConnectionAttempt;

      bool tryReconnect();
};

#endif
