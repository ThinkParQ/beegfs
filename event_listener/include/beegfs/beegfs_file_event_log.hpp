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

#include <stdio.h>
#include <string>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>

#define BEEGFS_EVENTLOG_FORMAT_MAJOR 1
#define BEEGFS_EVENTLOG_FORMAT_MINOR 0

#define BEEGFS_EVENTLOG_BDM_PORT 6000

namespace BeeGFS {

enum class FileEventType : uint32_t
{
   FLUSH       =  0,
   TRUNCATE    =  1,
   SETATTR     =  2,
   CLOSE_WRITE =  3,
   CREATE      =  4,
   MKDIR       =  5,
   MKNOD       =  6,
   SYMLINK     =  7,
   RMDIR       =  8,
   UNLINK      =  9,
   HARDLINK    = 10,
   RENAME      = 11,
   READ        = 12,
};


std::string to_string(const FileEventType& fileEvent)
{
   switch (fileEvent)
   {
      case FileEventType::FLUSH:
         return "Flush";
      case FileEventType::TRUNCATE:
         return "Truncate";
      case FileEventType::SETATTR:
         return "SetAttr";
      case FileEventType::CLOSE_WRITE:
         return "CloseAfterWrite";
      case FileEventType::CREATE:
         return "Create";
      case FileEventType::MKDIR:
         return "MKdir";
      case FileEventType::MKNOD:
         return "MKnod";
      case FileEventType::SYMLINK:
         return "Symlink";
      case FileEventType::RMDIR:
         return "RMdir";
      case FileEventType::UNLINK:
         return "Unlink";
      case FileEventType::HARDLINK:
         return "Hardlink";
      case FileEventType::RENAME:
         return "Rename";
      case FileEventType::READ:
         return "Read";
   }
   return "";
}

struct packet
{
    uint16_t formatVersionMajor;
    uint16_t formatVersionMinor;
    uint32_t size;
    uint64_t droppedSeq;
    uint64_t missedSeq;
    FileEventType type;
    std::string path;
    std::string entryId;
    std::string parentEntryId;
    std::string targetPath;
    std::string targetParentId;
};

class Reader
{
   public:
      explicit Reader(const char* data, const ssize_t size):
         position(data),
         end(data+size)
      {}

      template<typename T>
      T read()
      {
         return readRaw<T>();
      }

   protected:
      const char* position;
      const char* const end;

      template<typename T>
      T readRaw()
      {

         if (position + sizeof(T) >= end)
            throw std::out_of_range("Read past buffer end");

         const auto x = reinterpret_cast<const T*>(position);
         position += sizeof(T);
         return *x;
      }

};

template<>
bool Reader::read<bool>()
{
   return (0 != readRaw<uint8_t>());
}

template<>
uint32_t Reader::read<u_int32_t>()
{
   return le32toh(readRaw<uint32_t>());
}

template<>
uint64_t Reader::read<u_int64_t>()
{
   return le64toh(readRaw<uint64_t>());
}

template <>
std::string Reader::read<std::string>()
{
   const auto len = read<uint32_t>();

   if (position + len > end)
      throw std::out_of_range("Read past buffer end");

   const auto value = std::string(position, position + len);
   position += len + 1;
   return value;
}

template <typename T>
Reader& operator>>(Reader& r, T& value)
{
   value = r.read<T>();
   return r;
}

Reader& operator>>(Reader& r, FileEventType& value)
{
   value = static_cast<FileEventType>(r.read<std::underlying_type<FileEventType>::type>());
   return r;
}


class FileEventReceiver
{
   public:
      struct exception : std::runtime_error
      {
            using std::runtime_error::runtime_error;
      };

      enum class ReadErrorCode
      {
         Success,
         ReadFailed,
         VersionMismatch,
         InvalidSize
      };

      FileEventReceiver(const std::string& socketPath)
      {

         struct sockaddr_un listenAddr;

         listenAddr.sun_family = AF_UNIX;

         strncpy(listenAddr.sun_path, socketPath.c_str(), sizeof(listenAddr.sun_path));
         listenAddr.sun_path[sizeof(listenAddr.sun_path)-1] = '\0';

         unlink(listenAddr.sun_path);
         listenFD = socket(AF_UNIX, SOCK_SEQPACKET, 0);

         if (listenFD < 0)
            throw exception("Unable to create socket " + socketPath + ": "
                            + std::string(strerror(errno)));

         const auto len = strlen(listenAddr.sun_path) + sizeof(listenAddr.sun_family);
         if (bind(listenFD, (struct sockaddr *) &listenAddr, len) == -1)
         {
            throw exception("Unable to bind socket:" + std::string(strerror(errno)));
         }

         if (listen(listenFD, 1) == -1)
         {
            throw exception("Unable to listen: " + std::string(strerror(errno)));
         }
      }

      std::pair<ReadErrorCode, packet> read()
      {

        if (serverFD == -1)
        {
            serverFD = accept(listenFD, NULL, NULL);

            if (serverFD < 0)
               throw exception("Error accepting connection: " + std::string(strerror(errno)));
        }

        const auto recvRes = read_packet(serverFD);

        if (ReadErrorCode::Success != recvRes.first)
        {
            close(serverFD);
            serverFD = -1;
        }

        return recvRes;
      }

      std::vector<char> readSerializedData()
      {
         if (serverFD == -1)
         {
            serverFD = accept(listenFD, NULL, NULL);

            if (serverFD < 0)
               throw exception("Error accepting connection: " + std::string(strerror(errno)));
         }

         const auto recvRes = read_serialized_data(serverFD);

         if (recvRes.empty())
         {
            close(serverFD);
            serverFD = -1;
         }

         return recvRes;
      }

      std::pair<ReadErrorCode, packet> rawToPacket(char * data, size_t bytesRead)
      {
         packet res;
         Reader reader(data, bytesRead);

         reader >> res.formatVersionMajor
                >> res.formatVersionMinor;

         if (   res.formatVersionMajor != BEEGFS_EVENTLOG_FORMAT_MAJOR
             || res.formatVersionMinor <  BEEGFS_EVENTLOG_FORMAT_MINOR)
            return { ReadErrorCode::VersionMismatch, {} };

         reader >> res.size;

         if (res.size != bytesRead )
            return { ReadErrorCode::InvalidSize, {} };

         reader >> res.droppedSeq
                >> res.missedSeq
                >> res.type
                >> res.entryId
                >> res.parentEntryId
                >> res.path
                >> res.targetPath
                >> res.targetParentId;

         return { ReadErrorCode::Success, res };
      }

      ~FileEventReceiver()
      {
         close(serverFD);
         close(listenFD);
      }

   protected:
      int listenFD = -1;
      int serverFD = -1;

      std::pair<ReadErrorCode, packet> read_packet(int fd)
      {
         packet res;
         char data[65536];
         const auto bytesRead = recv(fd, data, sizeof(data), 0);

         const auto headerSize =   sizeof(packet::formatVersionMajor)
                                 + sizeof(packet::formatVersionMinor)
                                 + sizeof(packet::size);

         if (bytesRead < headerSize)
            return { ReadErrorCode::ReadFailed, {} };

         Reader reader(&data[0], bytesRead);

         reader >> res.formatVersionMajor
                >> res.formatVersionMinor;

         if (   res.formatVersionMajor != BEEGFS_EVENTLOG_FORMAT_MAJOR
             || res.formatVersionMinor <  BEEGFS_EVENTLOG_FORMAT_MINOR)
            return { ReadErrorCode::VersionMismatch, {} };

         reader >> res.size;

         if (res.size != bytesRead )
            return { ReadErrorCode::InvalidSize, {} };

         reader >> res.droppedSeq
                >> res.missedSeq
                >> res.type
                >> res.entryId
                >> res.parentEntryId
                >> res.path
                >> res.targetPath
                >> res.targetParentId;

         return { ReadErrorCode::Success, res };
      }

      std::vector<char> read_serialized_data(int fd)
      {
         packet res;
         std::vector<char> buf(65536);

         const auto bytesRead = recv(fd, buf.data(), buf.capacity(), 0);

         const auto headerSize =   sizeof(packet::formatVersionMajor)
                                 + sizeof(packet::formatVersionMinor)
                                 + sizeof(packet::size);

         if (bytesRead < headerSize)
         {
            buf.clear();
            return buf;
         }

         buf.resize(bytesRead); //resize it to actual bytes read

         Reader reader(buf.data(), bytesRead);

         reader >> res.formatVersionMajor
                >> res.formatVersionMinor;

         if (res.formatVersionMajor != BEEGFS_EVENTLOG_FORMAT_MAJOR
             || res.formatVersionMinor <  BEEGFS_EVENTLOG_FORMAT_MINOR)
         {
            buf.clear();
            return buf;
         }

         reader >> res.size;

         if (res.size != bytesRead )
         {
            buf.clear();
            return buf;
         }

         reader >> res.droppedSeq
                >> res.missedSeq
                >> res.type
                >> res.entryId
                >> res.parentEntryId
                >> res.path
                >> res.targetPath
                >> res.targetParentId;

         return buf;
      }

};

class BDMClient
{
   private:
      int sock_fd;

   public:
      BDMClient() : sock_fd(-1) {}

      bool connToBDM(const std::string& address, const int port)
      {
         //Crerating socket
         sock_fd = socket(AF_INET, SOCK_STREAM, 0);
         if(sock_fd < 0)
         {
            perror("socket create failed");
            return false;
         }

         const sockaddr_in server = {.sin_family = AF_INET,
                                     .sin_port = htons(port),
                                    };

         int opt = 1;

         if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
            static_cast<void *>(&opt), sizeof(opt)))
         {
            perror("Unable to set socket for hive index");
            return false;
         }
      
         if (inet_pton(AF_INET, address.c_str(), (void *)&server.sin_addr) <= 0)
         {
            perror("Unable to convert address");
            return false;
         }

         if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
         {
            perror("Unable to connect to socket");
            return false;
         }
         return true;

      }

      ~BDMClient()
      {
         if (sock_fd > 0)
            close(sock_fd);
      }
      int sendData (char * data, size_t dataSize);
};

/*
   Send data to the connected host
*/

int BDMClient::sendData(char *data, size_t dataSize)
{
   size_t total_sent = 0;
   int ret;

   iovec iov = {
     .iov_base = data,
     .iov_len = dataSize,
   };

   for (size_t send_rc = 0; total_sent < dataSize;)
   {
      msghdr msg = {
        .msg_name = 0,
        .msg_namelen = 0,
        .msg_iov = &iov,
        .msg_iovlen = 1,
      };

      // Send data
      send_rc = sendmsg(sock_fd, &msg, MSG_NOSIGNAL);
      if (send_rc < 0)
      {
         std::cout << "Send failed : ";
         return ret;
      }
      total_sent += send_rc;

      char *base = (char *)iov.iov_base;
      base += total_sent;
      iov.iov_base = base;
      iov.iov_len -= total_sent;
   }

   return 0;
}

} // namespace BeeGFS

#endif  // BEEGFSFILEVENTLOG_H_

