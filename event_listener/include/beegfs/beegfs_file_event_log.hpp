#ifndef BEEGFSFILEVENTLOG_H_
#define BEEGFSFILEVENTLOG_H_


#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdexcept>
#include <iostream>
#include <linux/limits.h>
#include <cstring>

#include <stdio.h>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>

#include <beegfs/seqpacket-reader-new-protocol.hpp>

#define BEEGFS_EVENTLOG_FORMAT_VERSION 2

namespace BeeGFS {

enum class FileEventType : uint32_t
{
   FLUSH       = 1,
   TRUNCATE    = 2,
   SETATTR     = 3,
   CLOSE_WRITE = 4,
   CREATE      = 5,
   MKDIR       = 6,
   MKNOD       = 7,
   SYMLINK     = 8,
   RMDIR       = 9,
   UNLINK      = 10,
   HARDLINK    = 11,
   RENAME      = 12,
   OPEN_READ   = 13,
   OPEN_WRITE  = 14,
   OPEN_READ_WRITE = 15,
   LAST_WRITER_CLOSED = 16
};


std::string to_string(const FileEventType& fileEvent);

struct packet
{
    uint16_t formatVersion;
    uint32_t eventFlags;
    uint64_t linkCount;
    FileEventType type;
    std::string path;
    std::string entryId;
    std::string parentEntryId;
    std::string targetPath;
    std::string targetParentId;
    uint32_t msgUserId;
    int64_t timestamp;
};

enum class PacketReadErrorCode
{
   Success,
   ReadFailed,
   VersionMismatch,
   InvalidSize
};

std::pair<PacketReadErrorCode, packet> read_packet_from_raw(void * data, size_t bytesRead);





class FileEventReceiver
{
   FileEventReceiverNewProtocol *receiver;

public:
   struct exception : std::runtime_error
   {
         using std::runtime_error::runtime_error;
   };

   std::pair<PacketReadErrorCode, packet> read();
   std::vector<char> readSerializedData();

   FileEventReceiver(FileEventReceiver const& other) = delete;

   FileEventReceiver(const std::string& socketPath);
   ~FileEventReceiver();
};

} // namespace BeeGFS

#endif  // BEEGFSFILEVENTLOG_H_

