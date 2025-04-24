#include <beegfs/beegfs_file_event_log.hpp>

#include <beegfs/seqpacket-reader-new-protocol.hpp>

namespace BeeGFS
{

class Reader
{
   public:
      explicit Reader(const void *data, const ssize_t size):
         position((const char *) data),
         end((const char *) data + size)
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
         if (position + sizeof(T) > end)
            throw std::out_of_range("Read past buffer end");

         T value;
         std::memcpy(&value, position, sizeof(T));
         position += sizeof(T);
         return value;
      }
};

template<>
inline bool Reader::read<bool>()
{
   return (0 != readRaw<uint8_t>());
}

template<>
inline uint32_t Reader::read<u_int32_t>()
{
   return le32toh(readRaw<uint32_t>());
}

template<>
inline uint64_t Reader::read<u_int64_t>()
{
   return le64toh(readRaw<uint64_t>());
}

template<>
inline int64_t Reader::read<int64_t>()
{
   return le64toh(readRaw<int64_t>());
}

template <>
inline std::string Reader::read<std::string>()
{
   const auto len = read<uint32_t>();

   if (position + len > end)
      throw std::out_of_range("Read past buffer end");

   const auto value = std::string(position, position + len);
   position += len + 1;
   return value;
}

template <typename T>
inline Reader& operator>>(Reader& r, T& value)
{
   value = r.read<T>();
   return r;
}

static inline Reader& operator>>(Reader& r, FileEventType& value)
{
   value = static_cast<FileEventType>(r.read<std::underlying_type<FileEventType>::type>());
   return r;
}


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
      case FileEventType::OPEN_READ:
         return "OpenRead";
      case FileEventType::OPEN_WRITE:
         return "OpenWrite";
      case FileEventType::OPEN_READ_WRITE:
         return "OpenReadWrite";
      case FileEventType::LAST_WRITER_CLOSED:
         return "LastWriterClosed";
   }
   return "";
}

std::pair<PacketReadErrorCode, packet> read_packet_from_raw(void * data, size_t bytesRead)
{
   packet res;
   Reader reader(data, bytesRead);

   reader >> res.formatVersion;

   if (res.formatVersion != BEEGFS_EVENTLOG_FORMAT_VERSION)
      return { PacketReadErrorCode::VersionMismatch, {} };

   reader >> res.eventFlags 
          >> res.linkCount
          >> res.type
          >> res.entryId
          >> res.parentEntryId
          >> res.path
          >> res.targetPath
          >> res.targetParentId
          >> res.msgUserId
          >> res.timestamp;

   return { PacketReadErrorCode::Success, res };
}

static std::vector<char> read_serialized_data(int fd)
{
   packet res;
   std::vector<char> buf(65536);

   const auto bytesRead = recv(fd, buf.data(), buf.capacity(), 0);

   const auto headerSize =   sizeof(packet::formatVersion);

   if (bytesRead < headerSize)
   {
      buf.clear();
      return buf;
   }

   buf.resize(bytesRead); //resize it to actual bytes read

   Reader reader(buf.data(), bytesRead);

   reader >> res.formatVersion;

   if (res.formatVersion != BEEGFS_EVENTLOG_FORMAT_VERSION)
   {
      buf.clear();
      return buf;
   }

   reader >> res.eventFlags
          >> res.linkCount
          >> res.type
          >> res.entryId
          >> res.parentEntryId
          >> res.path
          >> res.targetPath
          >> res.targetParentId
          >> res.msgUserId
          >> res.timestamp;

   return buf;
}

std::pair<PacketReadErrorCode, packet> FileEventReceiver::read()
{
   if (! receive_event(receiver))
   {
      std::pair<PacketReadErrorCode, packet> out;
      out.first = PacketReadErrorCode::ReadFailed;
      return out;
   }

   Read_Event event = get_event(receiver);
   return read_packet_from_raw(event.buffer, event.size);
}

std::vector<char> FileEventReceiver::readSerializedData()
{
   std::vector<char> out;

   if (! receive_event(receiver))
      return out;

   Read_Event event = get_event(receiver);

   out.resize(event.size);
   memcpy(out.data(), event.buffer, event.size);

   return out;
}



FileEventReceiver::FileEventReceiver(const std::string& socketPath)
{
   const char *address = socketPath.c_str();
   receiver = FileEventReceiverNewProtocolCreate(1, &address);
   if (! receiver)
      throw exception("Failed to init");
}

FileEventReceiver::~FileEventReceiver()
{
   if (receiver)
   {
      FileEventReceiverNewProtocolDestroy(receiver);
      receiver = nullptr;
   }
}

}
