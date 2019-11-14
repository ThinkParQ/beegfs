#include "FileEventLogger.h"

#include <common/app/config/AbstractConfig.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/storage/EntryInfo.h>

#include <mutex>

FileEventLogger::FileEventLogger(const std::string& address):
   sockFD(-1), droppedEventsSeq(0), missedEventsSeq(0), lastConnectionAttempt(true)
{
   if (address.substr(0, 5) != "unix:")
      throw InvalidConfigException("FileEventLogger supports only unix addresses");

   auto pathStr = address.substr(5);
   if (pathStr.size() >= sizeof(targetAddr.sun_path))
      throw InvalidConfigException("Bad address for FileEventLogger");

   targetAddr.sun_family = AF_UNIX;
   strcpy(targetAddr.sun_path, pathStr.c_str()); // NOLINT
}

FileEventLogger::~FileEventLogger()
{
   if (sockFD >= 0)
      close(sockFD);
}

void FileEventLogger::log(const FileEvent& event, const std::string& entryId,
                          const std::string& parentID, const std::string& targetParentID,
                          bool missedAnyEvent)
{
   std::unique_lock<Mutex> lock(mtx);

   if (sockFD < 0 && !tryReconnect())
   {
      droppedEventsSeq += 1;
      return;
   }

   if (missedAnyEvent)
      missedEventsSeq += 1;

   FileEventLogItem logItem = {droppedEventsSeq, missedEventsSeq,
                               {event.type,
                                entryId,
                                parentID,
                                event.path,
                                event.targetValid ? event.target : std::string(),
                                targetParentID
                               }
                              };

   char itemBuf[  sizeof (FileEventLogItem::droppedEventsSeq)
                + sizeof (FileEventLogItem::missedEventsSeq)
                + sizeof (FileEventLogItem::formatVersionMajor)
                + sizeof (FileEventLogItem::formatVersionMinor)
                + sizeof (FileEventLogItem::FileEventData::type)
                + 4096 + sizeof (u_int32_t)  // entryId
                + 4096 + sizeof (u_int32_t)  // parentEntryId
                + 4096 + sizeof (u_int32_t)  // path
                + 4096 + sizeof (u_int32_t)  // targetPath
                + 4096 + sizeof (u_int32_t)  // targetParentId
            ];

   const char* serializedItem = itemBuf;
   unsigned itemSize;

   std::unique_ptr<char[]> overflowItemBuf;

   do {
      Serializer ser(itemBuf, sizeof(itemBuf));
      ser % logItem;
      itemSize = ser.size();

      if (ser.good())
         break;

      LOG(EVENTLOGGER, WARNING, "Received a file event that exceeds the reserved buffer size.",
          ser.size());

      overflowItemBuf.reset(new (std::nothrow) char[ser.size()]);
      if (overflowItemBuf)
      {
         serializedItem = overflowItemBuf.get();

         ser = {overflowItemBuf.get(), ser.size()};
         ser % logItem;
         itemSize = ser.size();

         if (ser.good())
            break;
      }

      LOG(EVENTLOGGER, ERR, "Could not serialize file event.");
      droppedEventsSeq += 1;
      return;
   } while (false);

   ssize_t sendRes;

   sendRes = ::send(sockFD, serializedItem, itemSize, MSG_NOSIGNAL | MSG_DONTWAIT);
   if (sendRes < 0 && errno == EPIPE) {
      close(sockFD);
      sockFD = -1;
      if (tryReconnect()) {
         LOG(EVENTLOGGER, NOTICE, "Reconnected.");
         sendRes = ::send(sockFD, serializedItem, itemSize, MSG_NOSIGNAL | MSG_DONTWAIT);
      }
   }

   if (sendRes >= 0 && size_t(sendRes) == itemSize)
      return;

   if (sendRes >= 0)
   {
      LOG(EVENTLOGGER, ERR, "Incomplete send of file event.", sendRes, itemSize);
      close(sockFD);
      sockFD = -1;
      droppedEventsSeq += 1;
      return;
   }

   if (errno == EAGAIN || errno == EWOULDBLOCK)
   {
      droppedEventsSeq += 1;
      return;
   }

   if (errno == EMSGSIZE)
   {
      droppedEventsSeq += 1;
      LOG(EVENTLOGGER, WARNING, "File event too large to send.", itemSize);
      return;
   }

   LOG(EVENTLOGGER, ERR, "Send of file event failed.", sysErr);
   close(sockFD);
   sockFD = -1;
   droppedEventsSeq += 1;
}

bool FileEventLogger::tryReconnect()
{
   if (!lastConnectionAttempt.getIsZero() &&
         lastConnectionAttempt.elapsedMS() < 1000)
      return false;

   lastConnectionAttempt.setToNow();

   sockFD = socket(AF_UNIX, SOCK_SEQPACKET, 0);
   if (sockFD < 0)
   {
      LOG(EVENTLOGGER, ERR, "Failed to create FileEventLogger socket.", sysErr);
      return false;
   }

   if (fcntl(sockFD, F_SETFL, O_NONBLOCK) < 0)
   {
      close(sockFD);
      sockFD = -1;
      LOG(EVENTLOGGER, ERR, "Failed to create FileEventLogger socket.", sysErr);
      return false;
   }

   if (connect(sockFD, (sockaddr*) &targetAddr, sizeof(targetAddr)) < 0)
   {
      close(sockFD);
      sockFD = -1;
      LOG(EVENTLOGGER, ERR, "Failed to connect to FileEventLogger listener socket.", sysErr,
            ("sockPath", targetAddr.sun_path));
      return false;
   }

   return true;
}

void FileEventLogItem::serialize(const FileEventLogItem* obj, Serializer& ctx)
{

   const auto startOffset = ctx.size();
   ctx
         % FileEventLogItem::formatVersionMajor
         % FileEventLogItem::formatVersionMinor;
   auto sizeMarker = ctx.mark();
   ctx
         % uint32_t(0)   // placeholder for size
         % obj->droppedEventsSeq
         % obj->missedEventsSeq
         % obj->event;

   sizeMarker % uint32_t(ctx.size() - startOffset); // update packet size
}

void FileEventLogItem::FileEventData::serialize(const FileEventData* obj, Serializer& ctx)
{

   ctx % obj->type
       % obj->entryId
       % obj->parentId
       % obj->path
       % obj->targetPath
       % obj->targetParentId;
}
