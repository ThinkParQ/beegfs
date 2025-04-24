#include "FileEventLogger.h"

#include <common/app/config/AbstractConfig.h>
#include <common/memory/String.h>
#include <common/memory/Slice.h>
#include <common/memory/MallocString.h>
#include <common/memory/MallocBuffer.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/ArrayTypeTraits.h>
#include <common/toolkit/ArraySlice.h>
#include <common/storage/EntryInfo.h>
#include <common/threading/PThread.h>
#include <common/threading/LockedView.h>
#include <program/Program.h>
#include <pmq/pmq.hpp>

#include <sys/socket.h>
#include <sys/un.h>

#include <mutex>
#include <condition_variable>
#include <thread>
#include <pthread.h>
#include <cstdarg>

using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
using Milliseconds = std::chrono::milliseconds;

static TimePoint getNow()
{
   return std::chrono::steady_clock::now();
}

class Timer
{
   bool mSet = false;
   TimePoint mBeginTime;
   Milliseconds mDuration;

public:

   bool isSet() const
   {
      return mSet;
   }

   void clear()
   {
      mSet = false;
   }

   TimePoint getBeginTime() const
   {
      return mBeginTime;
   }

   TimePoint getEndTime() const
   {
      return mBeginTime + mDuration;
   }

   Milliseconds getRemainingTime(TimePoint now) const
   {
      return std::chrono::duration_cast<Milliseconds>(mBeginTime + mDuration - now);
   }

   bool hasElapsed(TimePoint now) const
   {
      return mSet && now >= getEndTime();
   }

   void restartFrom(TimePoint now)
   {
      mBeginTime = now;
      mSet = true;
   }

   void reset(TimePoint now, Milliseconds duration)
   {
      mDuration = duration;
      restartFrom(now);
   }

   Timer()
   {
      mDuration = Milliseconds(0);
   }

   Timer(Milliseconds duration)
   {
      mDuration = duration;
   }

   Timer(TimePoint now, Milliseconds duration)
   {
      reset(now, duration);
   }
};


class UnixAddr
{
   sockaddr_un addr = {};

public:
   const char *getPath() const
   {
      return addr.sun_path;
   }

   const struct sockaddr *getSockaddr() const
   {
      return (struct sockaddr *) &addr;
   }

   socklen_t size() const
   {
      return sizeof addr;
   }

   bool setPath(StringZ path)
   {
      RO_Slice source = path.sliceZ();
      WO_Slice dest = As_WO_Slice(addr.sun_path);
      if (source.sizeBytes() > dest.sizeBytes())
      {
         LOG(EVENTLOGGER, ERR, "Path too long, can't use as socket address:", path.c_str());
         return false;
      }
      sliceCopyFill0(dest, source);
      addr.sun_family = AF_UNIX;
      return true;
   }
};

struct SocketState
{
   UnixAddr unixAddr;

   FDHandle sockFd;

   // TODO: we try to avoid blocking when writing to the sockFd. So we might
   // have to set the socket non-blocking and/or keep track of the free space
   // in the kernel's socket buffer.
   int lastConnectError = -1;

   // sockReconnectTimer: time of next reconnect attempt
   // sockReconnectMsgTimer: when next to log a message that we're still reconnecting
   Timer sockReconnectTimer;
   Timer sockReconnectMsgTimer;

   bool haveConnected = false;
   bool haveError = false;
   bool haveEof = false;

   bool connected() const { return haveConnected; }

   [[nodiscard]] bool init(StringZ address);
   bool tryConnect();
};

bool SocketState::init(StringZ address)
{
   if (! address.startswith("unix:"_sz))
   {
      LOG(EVENTLOGGER, ERR,
            (std::string("Invalid FileEventLogger address: '")
            + std::string(address.c_str())
            + "FileEventLogger supports only unix addresses (starting with 'unix:')"));
      return false;
   }

   if (! unixAddr.setPath(address.offsetBytes(5)))
      return false;

   // Initially try to reconnect immediately
   // Every 10 minutes we will send a follow-up message to the log, indicating
   // that we're still connecting
   LOG(EVENTLOGGER, NOTICE, "Trying to connect to event listener socket...");
   auto now = getNow();
   sockReconnectTimer.reset(now, Milliseconds(0));
   sockReconnectMsgTimer.reset(now, Milliseconds(600000));
   return true;
}

bool SocketState::tryConnect()
{
   assert(! haveConnected);  // don't call when already connected
   auto now = getNow();

   if (sockReconnectMsgTimer.hasElapsed(now))
   {
      LOG(EVENTLOGGER, NOTICE, "still reconnecting...");
      sockReconnectMsgTimer.restartFrom(now);
      lastConnectError = -1; // allow another connect error msg too
   }

   sockFd.reset(socket(AF_UNIX, SOCK_SEQPACKET, 0));

   if (! sockFd.valid())
   {
      LOG(EVENTLOGGER, ERR, "Failed to create FileEventLogger socket.", sysErr);
      return false;
   }

   /* Note that we should try hard to avoid blocking anywhere since we have to
    * do multiple different time-critical things in the same thread.
    *
    * Note: Even though the socket is set to blocking mode, the connect()
    * _usually_ doesn't block since we're connecting to a Unix socket. On some
    * OSes, possibly including Linux, it will block though when the connection
    * queue of the server socket is full. (On other OSes, an error will be
    * returned immediately in this case).
    */
   if (connect(sockFd.get(), unixAddr.getSockaddr(), unixAddr.size()) == -1)
   {
      sockFd.close();

      // avoid spamming reconnect failures
      int e = errno;
      if (e != lastConnectError)
      {
         lastConnectError = e;
         LOG(EVENTLOGGER, WARNING, "Failed to connect to FileEventLogger listener socket.", sysErr,
               ("sockPath", unixAddr.getPath()));
         if (e == ENOENT)
         {
            LOG(EVENTLOGGER, WARNING, "(this probably means that nobody is listening on the configured socket path).");
         }
         else if (e == ECONNREFUSED)
         {
            LOG(EVENTLOGGER, WARNING, "(this probably means that a listener on the configured socket path terminated without cleaning up the socket file)");
         }
         LOG(EVENTLOGGER, NOTICE, "Note: still attempting to connect. Repetitions of this error won't be logged.");
      }

      // retry in 1000ms
      {
         sockReconnectTimer.reset(now, Milliseconds(1000));
      }

      return false;
   }

   // clear timer to indicate that socket works
   sockReconnectTimer.clear();

   haveConnected = true;
   LOG(EVENTLOGGER, NOTICE, "Reconnected.");
   return true;
}

// Packet types
// NOTE: for future extension, we can consider introducing messages to request
// certain capabilities. We allow for extensibility by introducing an error
// packet indicating that a request was not understood.
enum class PacketType : uint8_t
{
   Handshake_Request = 1,
   Handshake_Response,
   Request_Message_Newest,
   Send_Message_Newest,
   Request_Message_Range,
   Send_Message_Range,
   // Client requests message stream.
   // Server follows up with message stream.
   Request_Message_Stream_Start,
   // Server sends an (event) message to client
   Send_Message,
   // Client requests orderly connection close. Can be sent mid-stream.
   Close_Request,
   // Server informs client that connection will be closed.
   // This is the last message sent by the server.
   // Includes a ConnTerminateReason
   Send_Close,
};

enum class ConnTerminateReason
{
   Ack_Close,

   // This gets sent when the stream stopped without solicitation of the peer
   // that requested the stream. The connection is in an error state and will
   // be shut down by the server.
   Stream_Crashed,

   Socket_Error,

   Protocol_Error,

   Subscriber_Closed_Unexpectedly,
};

static const char *packetTypeString(PacketType packetType)
{
#define PTSTRING(x) case PacketType::x: return #x
   switch (packetType)
   {
      PTSTRING(Handshake_Request);
      PTSTRING(Handshake_Response);
      PTSTRING(Request_Message_Newest);
      PTSTRING(Send_Message_Newest);
      PTSTRING(Request_Message_Range);
      PTSTRING(Send_Message_Range);
      PTSTRING(Request_Message_Stream_Start);
      PTSTRING(Send_Message);
      PTSTRING(Close_Request);
      PTSTRING(Send_Close);
      default: return "(invalid packet type)";
   }
}


class PacketBuffer
{
   size_t mSize = 0;
   std::vector<char> mBuffer;

public:

   void reallocate(size_t capacity)
   {
      mBuffer.resize(capacity);
      mBuffer.shrink_to_fit();
   }

   void drop()
   {
      // better not use resize/shrink. The following should guarantee the
      // memory is released:
      mBuffer = std::vector<char>();
      mSize = 0;
   }

   void clear()
   {
      mSize = 0;
   }

   void *data()
   {
      return mBuffer.data();
   }

   size_t size() const
   {
      return mSize;
   }

   size_t capacity() const
   {
      return mBuffer.capacity();
   }

   void setSize(size_t size)
   {
      assert(size < mBuffer.capacity());
      mSize = size;
   }

   PacketBuffer& operator=(PacketBuffer&& other) noexcept
   {
      drop();
      std::swap(mSize, other.mSize);
      std::swap(mBuffer, other.mBuffer);
      return *this;
   }

   PacketBuffer()
   {
      reallocate(16 * 1024);
   }

   // I don't think we need copy constructor / copy assignment operator.
   PacketBuffer& operator=(PacketBuffer const& other) = delete;
   PacketBuffer(PacketBuffer const& other) = delete;

   PacketBuffer(PacketBuffer&& other) noexcept
   {
      *this = std::forward<PacketBuffer&&>(other);
   }
};



// Queue of packets -- used for both RX and TX queues. Any queue (RX or TX) has
// 2 sides (enqueue/dequeue). To enqueue, an API with 2 operations is needed to
// "get" (begin) enqueuing a free packet and then to submit it (end enqueuing).
// To dequeue, it's the same -- so 4 operations total for any queue (RX or TX).
//
// I chose to design the "begin" as an idempotent operation -- such that a
// second begin without an "end" in between will return the same packet,
// invalidating the first "begin".
class PacketQueue
{
   uint32_t count = 0;
   uint32_t rd = 0;  // read position
   uint32_t wr = 0;  // write position

   std::vector<PacketBuffer> queue;

   PacketBuffer *getPacket(uint32_t pos)
   {
      uint32_t index = pos & (count - 1);  // == (pos % count) provided that count == 2^n
      return &queue.at(index);
   }

public:
   void init(uint32_t packetcount)  // packetcount must be a power of 2
   {
      assert((packetcount & (packetcount - 1)) == 0);
      queue.resize(packetcount);
      count = packetcount;
   }

   PacketBuffer *enqueueBegin()
   {
      if (wr - rd == count)
         return nullptr;  // full
      return getPacket(wr);
   }

   void enqueueEnd()
   {
      assert(wr - rd < count);
      wr++;
   }

   PacketBuffer *dequeueBegin()
   {
      if (wr - rd == 0)
         return nullptr; //empty
      return getPacket(rd);
   }

   void dequeueEnd()
   {
      assert(wr - rd > 0);
      rd++;
   }
};



static void sendPackets(PacketQueue *txQueue, SocketState *socketState)
{
   int fd = socketState->sockFd.get();

   for (;;)
   {
      PacketBuffer *packet = txQueue->dequeueBegin();
      if (! packet)
         break;

      ssize_t n = ::send(fd, packet->data(), packet->size(), MSG_NOSIGNAL | MSG_DONTWAIT);

      if (n == -1)
      {
         int e = errno;
         if (e == EAGAIN || errno == EWOULDBLOCK)
         {
            // try again later
         }
         else if (e == EMSGSIZE)
         {
            LOG(EVENTLOGGER, WARNING, "Message too large to send over fileevent socket. Ignoring", packet->size());
         }
         else
         {
            LOG(EVENTLOGGER, ERR, "Failed to send message over fileevent socket.", sysErr);
            socketState->haveError = true;
         }
         break;
      }

      txQueue->dequeueEnd();
   }
}

static void recvPackets(PacketQueue *rxQueue, SocketState *socketState)
{
   int fd = socketState->sockFd.get();

   for (;;)
   {
      PacketBuffer *packet = rxQueue->enqueueBegin();
      if (! packet)
         break;

      ssize_t nr = ::recv(fd, packet->data(), packet->capacity(), MSG_DONTWAIT);

      if (nr == -1)
      {
         int e = errno;
         if (e == EAGAIN || errno == EWOULDBLOCK)
         {
         }
         else
         {
            LOG(EVENTLOGGER, ERR, "Failed to recv message over seqpacket socket.", sysErr);
            socketState->haveError = true;
         }
         break;
      }
      else if (nr == 0)
      {
         // peer closed their write end. NOTE: In principle a 0 return value can also
         // be the consequence of the peer sending a 0-sized packet. It's not obvious
         // to me how to distinguish the two cases easily. I'll posit that
         // 0-sized packets shouldn't be sent and will be interpreted as EOF.
         socketState->haveEof = true;
         break;
      }

      packet->setSize((size_t) nr);
      rxQueue->enqueueEnd();
   }
}







// Serializer for packet writing
struct PacketWriter
{
   PacketQueue *txQueue;
   PacketBuffer *packet;
   Serializer serializer;
};

static bool write_begin(PacketWriter& writer, PacketQueue *txQueue)
{
   PacketBuffer *packet = txQueue->enqueueBegin();
   if (! packet)
      return false;
   writer.txQueue = txQueue;
   writer.packet = packet;
   writer.serializer = Serializer(writer.packet->data(), writer.packet->capacity());
   return true;
}

static bool write_end(PacketWriter& writer)
{
   if (! writer.serializer.good())
      return false;
   writer.packet->setSize(writer.serializer.size());
   writer.txQueue->enqueueEnd();
   return true;
}

static void write_u8(PacketWriter& writer, uint8_t const& x)
{
   writer.serializer % x;
}

static void write_u16(PacketWriter& writer, uint16_t const& x)
{
   writer.serializer % x;
}

static void write_u32(PacketWriter& writer, uint32_t const& x)
{
   writer.serializer % x;
}

static void write_u64(PacketWriter& writer, uint64_t const& x)
{
   writer.serializer % x;
}

static void write_slice(PacketWriter& writer, RO_Slice slice)
{
   writer.serializer.putBlock(slice.data(), slice.sizeBytes());
}

static void write_packet_header(PacketWriter& writer, PacketType packetType)
{
   write_u8(writer, (uint8_t) packetType);
   write_slice(writer, RO_Slice("events", 7));
}


// Deserializer for packet reading. We currently "duplicate" the serializer code.
// I believe that, at least at the moment, this is better than adding a bad
// abstraction that overcomplicates things.
struct PacketReader
{
   PacketBuffer *packet;
   Deserializer deserializer;

   bool good() const { return deserializer.good(); }

   PacketType packetType;

   PacketReader(PacketBuffer *packet)
      // NOTE for the reader we consider the size of the packet instead of the capacity...
      // It's a bit of a weird asymmetrical abstraction
      : packet(packet), deserializer(packet->data(), packet->size())
   {}
};

static bool read_end(PacketReader& reader)
{
   reader.deserializer.parseEof();
   return reader.deserializer.good();
}

static void read_u8(PacketReader& reader, uint8_t& x)
{
   reader.deserializer % x;
}

static void read_u16(PacketReader& reader, uint16_t& x)
{
   reader.deserializer % x;
}

/* currently unused
static void read_u32(PacketReader& reader, uint32_t & x)
{
   reader.deserializer % x;
}
*/

static void read_u64(PacketReader& reader, uint64_t& x)
{
   reader.deserializer % x;
}

static void read_slice(PacketReader& reader, WO_Slice slice)
{
   reader.deserializer.getBlock(slice.data(), slice.sizeBytes());
}


// Message input stream -- buffers messages read from a PMQ
struct MessageStream
{
   PMQ_Reader_Handle reader;
   PMQ_Read_Result lastPmqError = PMQ_Read_Result_Success;
   // Currently buffering 1 message only.
   bool msgIsPresent = false;
   size_t msgSize = 0;
   uint64_t msgMsn = 0;
   MallocBuffer msgBuffer;

public:
   bool haveMessage() const { return msgIsPresent; }
   uint64_t getMsgMsn() const { return msgMsn; }
   const void *getMsgData() const { return msgBuffer.data(); }
   size_t getMsgSize() const { return msgSize; }
   PMQ_Read_Result getError() const { return lastPmqError; }
   bool haveError() const { return lastPmqError != PMQ_Read_Result_Success && lastPmqError != PMQ_Read_Result_EOF; }

   bool init(PMQ *pmq);
   void seek(uint64_t msn);
   bool checkMsg();
   void clearMsg() { msgIsPresent = false; }
};

bool MessageStream::init(PMQ *pmq)
{
   if (! msgBuffer.reset(16 * 1024))
      return false;
   reader = pmq_reader_create(pmq);
   return reader.get() != nullptr;
}

void MessageStream::seek(uint64_t msn)
{
   if (lastPmqError != PMQ_Read_Result_Success && lastPmqError != PMQ_Read_Result_EOF)
      return; // should we even get here?

   lastPmqError = pmq_reader_seek_to_msg(reader.get(), msn);

   switch (lastPmqError)
   {
   case PMQ_Read_Result_Success:
   break;
   case PMQ_Read_Result_EOF:
      assert(false);  // this shouldn't happen anymore
     // fallthrough, although we really should never get here.
   default:
      LOG(EVENTLOGGER, WARNING, "pmq_reader_seek_to_msg() returns", lastPmqError);
      LOG(EVENTLOGGER, WARNING, "Failed to seek to requested MSN:", msn);
   break;
   }
}

// Lock the queue and read the next message from the queue.
// While there is no message, we also make sure that the queue gets flushed as scheduled.
// We try to minimize the time where the queue is locked.
bool MessageStream::checkMsg()
{
   if (msgIsPresent)
      return true;

   msgSize = 0;
   msgMsn = pmq_reader_get_current_msn(reader);

   lastPmqError = pmq_read_msg(reader, msgBuffer.data(), msgBuffer.capacity(), &msgSize);

   switch (lastPmqError)
   {
      // The message was successfully read back.
      case PMQ_Read_Result_Success:
         msgIsPresent = true;
         LOG(EVENTLOGGER, SPAM, "pmq_read_msg() returns PMQ_Read_Result_Success");
         break;

      // How to handle all the other cases?
      case PMQ_Read_Result_Buffer_Too_Small:
         LOG(EVENTLOGGER, SPAM, "pmq_read_msg() returns PMQ_Read_Result_Buffer_Too_Small");
         break;

      case PMQ_Read_Result_EOF:
         // why have we been called then?
         //LOG(EVENTLOGGER, SPAM, "pmq_read_msg() returns PMQ_Read_Result_EOF");
         break;

      case PMQ_Read_Result_Out_Of_Bounds:
         LOG(EVENTLOGGER, DEBUG, "pmq_read_msg() returns PMQ_Read_Result_Out_Of_Bounds");
         break;

      case PMQ_Read_Result_IO_Error:
         LOG(EVENTLOGGER, ERR, "Failed to read message from queue: I/O error happened while reading message");
         break;

      case PMQ_Read_Result_Integrity_Error:
         LOG(EVENTLOGGER, ERR, "Failed to read message from queue: detected database corruption");
         break;

      default:
         assert(0);
         break;
   }

   return msgIsPresent;
}

struct Subscriber
{
   bool handshakeReceived = false;
   bool handshakeSent = false;
   bool streaming = false;
   bool haveMessageRangeRequest = false;
   bool haveMessageNewestRequest = false;
   bool closeRequested = false;
   bool terminated = false;

   uint16_t peerMajorVersion = 0;  // valid if handshakeReceived
   uint16_t peerMinorVersion = 0;

   FileEventLoggerIds ids;

   MessageStream messageStream;
   SocketState socketState;

   PacketQueue rxQueue;
   PacketQueue txQueue;
};


__attribute__((format(printf, 2, 3)))
static void malformedPacket(Subscriber *s, const char *fmt, ...); // NOLINT


static bool packetBegin(Subscriber *s, PacketReader& reader)
{
   uint8_t packetType;
   char check[7];

   read_u8(reader, packetType);
   read_slice(reader, WO_Slice(check, sizeof check));

   if (! reader.good())
   {
      malformedPacket(s, "Invalid packet received: Failed to parse header");
      return false;
   }

   if (strcmp(check, "events") != 0)
   {
      malformedPacket(s, "Invalid packet received: mismatch of magic string");
      return false;
   }

   reader.packetType = (PacketType) packetType;
   return true;
}

static bool packetEnd(Subscriber *s, PacketReader& reader)
{
   if (! read_end(reader))
   {
      malformedPacket(s, "too long. Expected end of %s",
            packetTypeString(reader.packetType));
      return false;
   }
   return true;
}



[[nodiscard]]
static bool resetSubscriber(Subscriber *s, PMQ *pmq, StringZ address, FileEventLoggerIds ids)
{
   // clean way to make a new object without new'ing.
   // It may be a bit unfortunate that the packet queues will be reallocated.
   *s = Subscriber();

   if (! s->messageStream.init(pmq))
      goto error;

   if (! s->socketState.init(address))
      goto error;

   s->ids = ids;

   s->rxQueue.init(8);
   s->txQueue.init(8);

   return true;

error:
   LOG(EVENTLOGGER, ERR, "Failed to reset Subscriber state");
   return false;
}

// helper function to process a single packet.
// returns true if packet can be considered "processed" i.e. thrown away.
static void processPacket(Subscriber *s, PacketBuffer *packet)
{
   PacketReader reader(packet);

   if (! packetBegin(s, reader))
      return;

   if (s->handshakeReceived)
   {
      if (reader.packetType == PacketType::Handshake_Request)
      {
         malformedPacket(s, "Duplicate handshake received from subscriber");
         return;
      }
   }
   else
   {
      if (reader.packetType != PacketType::Handshake_Request)
      {
         malformedPacket(s, "Invalid packet received from subscriber: expected handshake packet");
         return;
      }
   }

   switch (reader.packetType)
   {
      case PacketType::Handshake_Request:
      {
         uint16_t major;
         uint16_t minor;

         read_u16(reader, major);
         read_u16(reader, minor);

         if (! packetEnd(s, reader))
         {
            malformedPacket(s, "Handshake request packet invalid");
            return;
         }

         if (major != 2 || minor != 0)
         {
            char version[16];
            snprintf(version, sizeof version, "%u.%u", major, minor);
            LOG(EVENTLOGGER, WARNING, "Unsupported protocol version requested subscriber: 2.0 required, got:", version);
         }

         s->peerMajorVersion = major;
         s->peerMinorVersion = minor;
         s->handshakeReceived = true;
      }
      break;
      case PacketType::Request_Message_Newest:
      {
         if (! packetEnd(s, reader))
         {
            malformedPacket(s, "Invalid Request_Message_Range packet");
            return;
         }

         s->haveMessageNewestRequest = true;
      }
      break;
      case PacketType::Request_Message_Range:
      {
         if (! packetEnd(s, reader))
         {
            malformedPacket(s, "Invalid Request_Message_Range packet");
            return;
         }

         s->haveMessageRangeRequest = true;
      }
      break;
      case PacketType::Request_Message_Stream_Start:
      {
         uint64_t msn;

         read_u64(reader, msn);

         if (! packetEnd(s, reader))
            return;

         // Position reader right away (this is a synchronous operation)
         // Otherwise we'd need more state variables.
         s->messageStream.seek(msn);
         s->streaming = true;
      }
      break;
      case PacketType::Close_Request:
      {
         if (! packetEnd(s, reader))
            return;

         s->closeRequested = true;
      }
      break;
      default:
      break;
   }
   return;
}

static void subscriberTerminate(Subscriber *s, ConnTerminateReason reason)
{
   if (s->terminated)
      return;  // send only 1 terminate packet

   // send an error packet
   if (PacketWriter writer;
      write_begin(writer, &s->txQueue))
   {
      write_packet_header(writer, PacketType::Send_Close);
      write_u8(writer, (uint8_t) reason);
      write_end(writer);
   }

   s->terminated = true;
}

__attribute__((format(printf, 2, 3)))
static void malformedPacket(Subscriber *s, const char *fmt, ...)  // NOLINT
{
   va_list ap;
   va_start(ap, fmt);
   {
      char msg[128];
      vsnprintf(msg, sizeof msg, fmt, ap);
      msg[sizeof msg - 1] = 0;
      LOG(EVENTLOGGER, WARNING, msg);
   }
   va_end(ap);

   subscriberTerminate(s, ConnTerminateReason::Protocol_Error);
}

static void subscriberDoNonIOWork(Subscriber *s)
{
   if (! s->handshakeSent)
   {
      if (PacketWriter writer;
         write_begin(writer, &s->txQueue))
      {
         write_packet_header(writer, PacketType::Handshake_Response);
         write_u16(writer, 2);  // major
         write_u16(writer, 0);  // minor
         write_u32(writer, s->ids.nodeId);
         write_u16(writer, s->ids.buddyGroupId);
         if (write_end(writer))
         {
            s->handshakeSent = true;
         }
      }
   }

   if (! s->handshakeSent)
   {
      return;
   }

   for (;;)
   {
      PacketBuffer *packet = s->rxQueue.dequeueBegin();
      if (! packet)
         break; // all packets processed, queue empty
      processPacket(s, packet);
      s->rxQueue.dequeueEnd();
   }

   if (s->messageStream.haveError())
   {
      subscriberTerminate(s, ConnTerminateReason::Stream_Crashed);
      return;
   }

   if (s->closeRequested)
   {
      subscriberTerminate(s, ConnTerminateReason::Ack_Close);
      return;
   }

   if (s->haveMessageRangeRequest)
   {
      if (PacketWriter writer;
         write_begin(writer, &s->txQueue))
      {
         PMQ_Reader *pmq_reader = s->messageStream.reader.get();
         PMQ *pmq = pmq_reader_get_pmq(pmq_reader);

         PMQ_Persist_Info persistInfo = pmq_get_persist_info(pmq);
         uint64_t new_msn = persistInfo.wal_msn;
         uint64_t old_msn = pmq_reader_find_old_msn(pmq_reader);

         write_packet_header(writer, PacketType::Send_Message_Range);
         write_u64(writer, new_msn);
         write_u64(writer, old_msn);

         if (write_end(writer))
         {
            s->haveMessageRangeRequest = false;
         }
      }
   }
   else if (s->haveMessageNewestRequest)
   {
      if (PacketWriter writer;
         write_begin(writer, &s->txQueue))
      {
         PMQ_Reader *pmq_reader = s->messageStream.reader.get();
         PMQ *pmq = pmq_reader_get_pmq(pmq_reader);

         PMQ_Persist_Info persistInfo = pmq_get_persist_info(pmq);
         uint64_t new_msn = persistInfo.wal_msn;

         write_packet_header(writer, PacketType::Send_Message_Newest);
         write_u64(writer, new_msn);

         if (write_end(writer))
         {
            s->haveMessageNewestRequest = false;
         }
      }
   }

   if (s->streaming)
   {
      // While there are more messages to read and the packet tx queue isn't
      // full, send more packets.
      // TODO more elaborate data flow control?
      for (;
            s->messageStream.checkMsg();
            s->messageStream.clearMsg())
      {
         PacketWriter writer;
         if (! write_begin(writer, &s->txQueue))
            break;

         uint64_t msn = s->messageStream.getMsgMsn();
         const void *msgData = s->messageStream.getMsgData();
         size_t msgSize = s->messageStream.getMsgSize();

         write_packet_header(writer, PacketType::Send_Message);
         write_u64(writer, msn);
         write_u16(writer, msgSize);
         write_slice(writer, RO_Slice(msgData, msgSize));

         if (! write_end(writer))
         {
            // can this ever happen?
            LOG(EVENTLOGGER, ERR, "Failed to serialize message of size", msgSize);
         }
      }
   }
}


static void subscriberDoWork(Subscriber *s)
{
   assert(! s->terminated);  // we should not have been called.

   if (! s->socketState.connected())
   {
      if (! s->socketState.tryConnect())
         return;
   }

   recvPackets(&s->rxQueue, &s->socketState);

   subscriberDoNonIOWork(s);

   sendPackets(&s->txQueue, &s->socketState);

   if (s->socketState.haveEof)
   {
      if (! s->closeRequested)
      {
         subscriberTerminate(s, ConnTerminateReason::Subscriber_Closed_Unexpectedly);
         return;
      }
   }

   if (s->socketState.haveError)
   {
      subscriberTerminate(s, ConnTerminateReason::Socket_Error);
      return;
   }
}


// State shared between enqueuer threads and the "EventQ-Worker" thread.
struct EventLoggerShared
{
   std::condition_variable readerCond;  // the unique reader thread (network bound) is waiting on this
   std::condition_variable writerCond;  // any event producer thread is waiting on this

   int numWritersWaiting = 0;
   int numReadersWaiting = 0;

   Timer flushTimer;

   bool shuttingDown = false;  // termination signal for worker thread
};


// State of the worker thread which has multiple responsibilities:
//  - flushing/syncing the PMQ so that new messages are safely persisted to disk.
//  - reading messages from the PMQ and forwarding them to downstream services
struct EventLoggerWorker
{
   PMQ *pmq = nullptr;
   MutexProtected<EventLoggerShared> *shared = nullptr;
   FileEventLoggerIds ids;

   MallocString address;

   // only 1 simultaneous subscriber currently.
   Subscriber subscriber;
};

struct EventLoggerWorkerParams
{
   PMQ *pmq;
   MutexProtected<EventLoggerShared> *shared;
   FileEventLoggerIds ids;
   StringZ address;
};

[[nodiscard]]
static bool workerResetSubscriber(EventLoggerWorker *worker)
{
   return resetSubscriber(&worker->subscriber,
         worker->pmq, worker->address, worker->ids);
}

static bool initWorker(EventLoggerWorker *worker, EventLoggerWorkerParams const& params)
{
   worker->pmq = params.pmq;
   worker->shared = params.shared;
   if (! worker->address.reset(params.address))
      return false;
   worker->ids = params.ids;
   return workerResetSubscriber(worker);
}

// Flush/sync the PMQ
static void workerFlush(EventLoggerWorker *worker)
{
   {
      auto shared = worker->shared->lockedView();
      shared->flushTimer.clear();
   }

   //LOG_DBG(EVENTLOGGER, SPAM, "worker thread flush");
   if (! pmq_sync(worker->pmq))
      LOG_DBG(EVENTLOGGER, ERR, "Failed to flush PMQ: pmq_sync() failed");

   {
      auto shared = worker->shared->lockedView();
      if (shared->numWritersWaiting > 0)
         shared->writerCond.notify_all();
   }
}

struct Worker_Wait_Result
{
   bool shuttingDown = false;
   bool haveMessage = false;
   bool mustFlush = false;
   bool mustReconnect = false;
};

static void workerWait(EventLoggerWorker *worker, Subscriber const *subscriber, Worker_Wait_Result *result)
{
   *result = Worker_Wait_Result();

   auto now = getNow();

   LockedView<EventLoggerShared> shared = worker->shared->lockedView();

   if (shared->shuttingDown)
   {
      result->shuttingDown = true;
      return;
   }

   result->haveMessage = subscriber->streaming && ! pmq_reader_eof(subscriber->messageStream.reader.get());
   result->mustFlush = shared->flushTimer.hasElapsed(now);
   result->mustReconnect = subscriber->socketState.sockReconnectTimer.hasElapsed(now);

   if (result->haveMessage) return;
   if (result->mustFlush) return;
   if (result->mustReconnect) return;

   // Nothing to do.
   // indicate to writer threads that we want to be woken up
   // TODO: is this a necessary optimization? Need to check, maybe condition
   // variables know on their own if anybody is waiting -- so the enqueuer threads
   // could always call notify_one() unconditionally.

   ++ shared->numReadersWaiting;
   {
      TimePoint waitEndTime = now + Milliseconds(50);

      if (shared->flushTimer.isSet())
         waitEndTime = std::min(waitEndTime, shared->flushTimer.getEndTime());

      if (subscriber->socketState.sockReconnectTimer.isSet())
         waitEndTime = std::min(waitEndTime, subscriber->socketState.sockReconnectTimer.getEndTime());

      shared->readerCond.wait_until(shared.get_unique_lock(), waitEndTime);
   }
   -- shared->numReadersWaiting;
}

static void workerThreadRoutine(EventLoggerWorker *worker)
{
   LOG(EVENTLOGGER, NOTICE, "EventQ-Worker thread starting");
   for (;;)
   {
      Worker_Wait_Result waitResult;

      workerWait(worker, &worker->subscriber, &waitResult);

      if (waitResult.shuttingDown)
      {
         break;
      }

      if (waitResult.mustFlush)
      {
         workerFlush(worker);
      }

      subscriberDoWork(&worker->subscriber);

      if (worker->subscriber.terminated)
      {
         if (! workerResetSubscriber(worker))
         {
            // What should we do? Probably this shouldn't even happen so...
         }
      }
   }
   LOG(EVENTLOGGER, NOTICE, "EventQ-Worker thread terminating");
}


// I think the PThread class is... weird. So I'm working a bit around what I
// perceive as problems. For example Pthread it doesn't allow us to easily
// check if a thread was started, and trying to join will throw an exception if
// nothing was started. The actual error values from pthread_*() are not
// reported at least in some cases.
//
// And to be able to pass arguments to the started thread, PThread design
// assumes that it's a good idea to require the implementation to inherit from
// PThread. I disagree, and favour separating the functional implementation
// from the thread state management.

class EventQ_Worker_Thread : public PThread
{
   EventLoggerWorker worker;
   bool mWorkerValid = false;
   bool mThreadRunning = false;

public:

   void run() override
   {
      if (mWorkerValid)
         workerThreadRoutine(&worker);
   }

   void startWorkerThread(EventLoggerWorkerParams const& params)
   {
      mWorkerValid = initWorker(&worker, params);

      start();
      mThreadRunning = true;
   }

   void join()
   {
      PThread::join();
      mThreadRunning = false;
   }

   bool joinable()
   {
      return mThreadRunning;
   }

   EventQ_Worker_Thread() : PThread("EventQ-Worker")
   {
   }
};

static PMQ *initPmq()
{
   auto config = Program::getApp()->getConfig();

   std::string dirpath = config->getFileEventPersistDirectory();

   if (dirpath.empty())
   {
      dirpath = config->getStoreMetaDirectory();

      if (dirpath.empty())
      {
         LOG(EVENTLOGGER, ERR, std::string("Unexpected: storeMetaDirectory configuration is empty"));
         return nullptr;
      }

      if (dirpath.back() != '/')
         dirpath += "/";

      dirpath += "eventq";
   }

   int64_t persistSize = config->getFileEventPersistSize();
   if (persistSize < 0)
   {
      LOG(EVENTLOGGER, ERR, std::string("sysFileEventPersistSize is negative."));
      return nullptr;
   }

   PMQ_Init_Params params = {};
   params.create_size = persistSize;
   params.basedir_path = dirpath.c_str();

   return pmq_create(&params);
}

struct FileEventLogger
{
   PMQ_Handle pmq;

   MutexProtected<EventLoggerShared> shared;

   EventQ_Worker_Thread workerThread;
};





FileEventLogger *createFileEventLogger(FileEventLoggerParams const& params)
{
   assert(! params.address.empty());
   StringZ address = StringZ::fromZeroTerminated(params.address.c_str(), params.address.size());

   std::unique_ptr<FileEventLogger, decltype(&destroyFileEventLogger)> logger { nullptr, destroyFileEventLogger };

   logger.reset(new FileEventLogger);

   logger->pmq = initPmq();

   if (! logger->pmq)
   {
      LOG(EVENTLOGGER, ERR, "Failed to initialize File Event Queue");
      throw std::bad_alloc();
   }

   EventLoggerWorkerParams workerParams = {};
   workerParams.pmq = logger->pmq.get();
   workerParams.shared = &logger->shared;
   workerParams.address = address;
   workerParams.ids = params.ids;

   logger->workerThread.startWorkerThread(workerParams);

   return logger.release();
}

void destroyFileEventLogger(FileEventLogger *logger)
{
   LOG(EVENTLOGGER, NOTICE, "Requesting EventQ-Worker thread to shut down");
   {
      LockedView<EventLoggerShared> shared = logger->shared.lockedView();
      shared->shuttingDown = true;
      shared->readerCond.notify_one();
   }

   if (logger->workerThread.joinable())
   {
      try {
         logger->workerThread.join();
      }
      catch (...) {
         // An exception would mean that the thread wasn't started,
         // contradicting the joinable() test.
         assert(false);
      }
   }

   delete logger;
}

struct FileEventLogItem
{
   const static constexpr uint16_t formatVersion = 2;
   uint32_t eventFlags;
   uint64_t numHardlinks;
   struct FileEventData
   {
      FileEventType type;
      std::string entryId;
      std::string parentId;
      std::string path;
      std::string targetPath;
      std::string targetParentId;
      unsigned msgUserId;
      int64_t timestamp;

      static void serialize(const FileEventData* obj, Serializer& ctx);
   };

   FileEventData event;

   static void serialize(const FileEventLogItem* obj, Serializer& ctx);
};

static constexpr size_t EVENT_BUFFER_SIZE = sizeof (FileEventLogItem::numHardlinks)
                                          + sizeof (FileEventLogItem::formatVersion)
                                          + sizeof (FileEventLogItem::eventFlags)
                                          + sizeof (FileEventLogItem::FileEventData::type)
                                          + sizeof (FileEventLogItem::FileEventData::msgUserId)
                                          + sizeof (FileEventLogItem::FileEventData::timestamp)
                                          + 4096 + sizeof (u_int32_t)   // entryId
                                          + 4096 + sizeof (u_int32_t)   // parentEntryId
                                          + 4096 + sizeof (u_int32_t)   // path
                                          + 4096 + sizeof (u_int32_t)   // targetPath
                                          + 4096 + sizeof (u_int32_t);  // targetParentId


void logEvent(FileEventLogger *logger, const FileEvent& event, const EventContext& eventCtx)
{
   LockedView<EventLoggerShared> shared = logger->shared.lockedView();

   FileEventLogItem logItem = {
                              eventCtx.eventFlags,
                              eventCtx.linkCount,
                               {event.type,
                                eventCtx.entryId,
                                eventCtx.parentId,
                                event.path,
                                event.targetValid ? event.target : std::string(),
                                eventCtx.targetParentId,
                                eventCtx.msgUserId,
                                eventCtx.timestamp
                               }
                              };

   std::vector<char> itemBuf(EVENT_BUFFER_SIZE);
   unsigned itemSize;

   do {
      Serializer ser(itemBuf.data(), itemBuf.size());
      ser % logItem;
      itemSize = ser.size();

      if (ser.good())
         break;

      LOG(EVENTLOGGER, WARNING, "Received a file event that exceeds the reserved buffer size.",
          ser.size());

      itemBuf.resize(ser.size());
      ser % logItem;
      itemSize = ser.size();

      if (ser.good())
         break;

      LOG(EVENTLOGGER, ERR, "Could not serialize file event.");
      return;
   } while (false);

   while (! pmq_enqueue_msg(logger->pmq, itemBuf.data(), itemSize))
   {
      LOG(EVENTLOGGER, SPAM, "Producer flushing queue");
      // TODO make sure it was actually due to a full queue, and not some other error.
      if (! pmq_sync(logger->pmq))
      {
         LOG_DBG(EVENTLOGGER, ERR, "Failed to flush PMQ: pmq_sync() failed");
         return;
      }
   }

   LOG(EVENTLOGGER, SPAM, "Enqueued message:", itemSize);
   // make sure that new messages get flushed regularly.
   {
      if (!shared->flushTimer.isSet())
      {
         auto now = getNow();
         shared->flushTimer.reset(now, Milliseconds(1000));
      }
   }

   if (shared->numReadersWaiting > 0)
   {
      //LOG(EVENTLOGGER, SPAM, "Notifying reader");
      shared->readerCond.notify_all();
   }
}

void FileEventLogItem::serialize(const FileEventLogItem* obj, Serializer& ctx)
{
   ctx
         % FileEventLogItem::formatVersion
         % obj->eventFlags
         % obj->numHardlinks
         % obj->event;
}

void FileEventLogItem::FileEventData::serialize(const FileEventData* obj, Serializer& ctx)
{
   ctx % obj->type
       % obj->entryId
       % obj->parentId
       % obj->path
       % obj->targetPath
       % obj->targetParentId
       % obj->msgUserId
       % obj->timestamp;
}



EventContext makeEventContext(EntryInfo* entryInfo, std::string parentId, unsigned msgUserId,
      std::string targetParentId, unsigned linkCount, bool isSecondary)
{
   uint32_t eventFlags = EventContext::EVENTFLAG_NONE;
   if (entryInfo->getIsBuddyMirrored())
      eventFlags |= EventContext::EVENTFLAG_MIRRORED;
   if (isSecondary)
      eventFlags |= EventContext::EVENTFLAG_SECONDARY;

   EventContext eventContext = {};
   eventContext.entryId = entryInfo->getEntryID();
   eventContext.parentId = std::move(parentId);
   eventContext.msgUserId = msgUserId;
   eventContext.targetParentId = std::move(targetParentId);
   eventContext.linkCount = linkCount;
   eventContext.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
   eventContext.eventFlags = eventFlags;

   return eventContext;
}
