#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <optional>


#include <beegfs/seqpacket-reader-new-protocol.hpp>


namespace BeeGFS
{


static void fatal_f(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   fprintf(stderr, "FATAL_ERROR: ");
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
   va_end(ap);
   exit(1);
}

static void msg_fv(const char *fmt, va_list ap)
{
   vfprintf(stderr, fmt, ap);
   fprintf(stderr, "\n");
}

static void __attribute__((format(printf, 1, 2))) msg_f(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   msg_fv(fmt, ap);
   va_end(ap);
}




struct Time
{
   uint64_t nanoseconds = 0;

   Time(uint64_t nanoseconds)
      : nanoseconds(nanoseconds)
   {
   }

   Time(struct timespec ts)
   {
      nanoseconds = ts.tv_nsec + ts.tv_sec * 1000000000ull;
   }

   Time operator-(Time const other) const
   {
      return Time(nanoseconds - other.nanoseconds);
   }

   bool operator<(Time const& other) { return nanoseconds < other.nanoseconds; }
   bool operator>(Time const& other) { return nanoseconds > other.nanoseconds; }
   bool operator<=(Time const& other) { return nanoseconds <= other.nanoseconds; }
   bool operator>=(Time const& other) { return nanoseconds >= other.nanoseconds; }
   bool operator==(Time const& other) { return nanoseconds == other.nanoseconds; }
   bool operator!=(Time const& other) { return nanoseconds != other.nanoseconds; }

   uint64_t to_millis() const
   {
      return nanoseconds / 1000000ull;
   }

   static Time Milliseconds(uint64_t millis)
   {
      return Time(millis * 1000000ull);
   }
};

static Time get_thread_time()
{
   struct timespec ts;
   if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) < 0)
   {
      fatal_f("Failed to clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...): %s",
            strerror(errno));
   }
   return Time(ts);
}



// Packet types
// NOTE: for future extension, we can consider introducing messages to request
// certain capabilities. We allow for extensibility by introducing an error
// packet indicating that a request was not understood.
enum class Packet_Type : uint8_t
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
   Request_Close,
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

static const char *packet_type_string(Packet_Type packetType)
{
#define PTSTRING(x) case Packet_Type::x: return #x
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
      PTSTRING(Request_Close);
      PTSTRING(Send_Close);
      default: return "(invalid packet type)";
   }
}


enum Report_Type
{
   Report_Type_None,
   Report_Type_Print_Message,
   Report_Type_Interactive_Count,
};


struct FileEventReceiverOptions
{
   const char *unix_socket_path = nullptr;

   Report_Type report_type = Report_Type_None;

   std::optional<uint64_t> startmsn;
   std::optional<uint64_t> nmsgs;
};


struct Packet_Buffer
{
   char data[1024];
   size_t size = 0;
   // also doubling as a packet writer for now
   bool bad = false;
};



struct Conn_State
{
   FileEventReceiverOptions options;
   bool handshake_sent = false;
   uint8_t server_version_major = 0;
   uint8_t server_version_minor = 0;
   uint32_t meta_id = 0;
   uint16_t meta_mirror_id = 0;
   bool handshake_received = false;
   unsigned conn_id = 0;
   int client_sock = -1;
   uint64_t curmsn = 0;
   uint64_t nmsgs = 0;
   Time last_time = 0;

   Packet_Buffer receive_packet;

   bool requested_msn = false;
   bool requested_stream = false;

   std::optional<uint64_t> startmsn;
};

void write_data(Packet_Buffer *packet, const void *data, size_t size)
{
   if (packet->size + size >= sizeof packet->data)
   {
      packet->bad = true;
      return;
   }
   memcpy(packet->data + packet->size, data, size);
   packet->size += size;
}

void write_header(Packet_Buffer *packet, Packet_Type type)
{
   assert(packet->size == 0);
   packet->data[0] = (char) type;
   memcpy(packet->data + 1, "events", 7);
   packet->size = 8;
}

void write_u8(Packet_Buffer *packet, uint8_t value)
{
   write_data(packet, &value, sizeof value);
}

void write_u16(Packet_Buffer *packet, uint16_t value)
{
   write_data(packet, &value, sizeof value);
}

void write_u64(Packet_Buffer *packet, uint64_t value)
{
   write_data(packet, &value, sizeof value);
}

static void report_count(Conn_State *conn)
{
   Time now = get_thread_time();
   if (now - conn->last_time >= Time::Milliseconds(10))
   {
      conn->last_time = now;
      msg_f("\x1b[A\x1b[K" "msg #%" PRIu64, conn->curmsn);
   }
}

static bool send_packet(Conn_State *conn, Packet_Buffer const *packet)
{
   if (packet->bad)
   {
      msg_f("Can't send: bad packet");
      return false;
   }

   ssize_t nr = send(conn->client_sock, packet->data, packet->size, 0);

   if (nr == -1)
   {
      msg_f("Failed to send packet of size %zu: %s", packet->size, strerror(errno));
      return false;
   }

   assert(nr >= 0);
   if ((size_t) nr != packet->size)
   {
      msg_f("Failed to send packet, short write");
      return false;
   }

   return true;
}

static bool do_message(Conn_State *conn)
{
   if (! conn->handshake_sent)
   {
      uint16_t protocol_version_major = 2;
      uint16_t protocol_version_minor = 0;
      Packet_Buffer packet;
      write_header(&packet, Packet_Type::Handshake_Request);
      write_u16(&packet, protocol_version_major);
      write_u16(&packet, protocol_version_minor);
      if (! send_packet(conn, &packet))
         return false;
      conn->handshake_sent = true;
   }

   if (conn->handshake_received)
   {
      if (! conn->startmsn.has_value())
      {
         if (! conn->requested_msn)
         {
            Packet_Buffer packet;
            write_header(&packet, Packet_Type::Request_Message_Range);
            if (! send_packet(conn, &packet))
               return false;
            //msg_f("send Request_Message_Range");
            conn->requested_msn = true;
         }
      }
      else if (! conn->requested_stream)
      {
         Packet_Buffer packet;
         write_header(&packet, Packet_Type::Request_Message_Stream_Start);
         write_u64(&packet, conn->startmsn.value());
         if (! send_packet(conn, &packet))
            return false;
         //msg_f("send Request_Message_Stream_Start");
         conn->curmsn = conn->startmsn.value();
         conn->requested_stream = true;
      }
   }

   ssize_t nr = read(conn->client_sock,
         conn->receive_packet.data, sizeof conn->receive_packet.data);

   if (nr < 0)
   {
      fatal_f("Error from read(): ", strerror(errno));
   }

   if (nr == 0)
   {
      msg_f("Conn %u was shut down", conn->conn_id);
      return false;
   }

   conn->receive_packet.size = nr;
   char *buf = conn->receive_packet.data;

   if (nr < 8 || memcmp(buf + 1, "events", 7) != 0)
   {
      for (ssize_t i = 0; i < nr; i++)
      {
         if (buf[i] < 32 || buf[i] > 126)
            printf(" 0x%.2x", buf[i]);
         else
            putchar(buf[i]);
      }
      printf("\n");
      msg_f("Received bad packet of size %zd: %.*s!", nr, (int) nr, buf);
      return false;
   }

   Packet_Type packet_type = (Packet_Type) buf[0];

   //msg_f("Received packet (type %d) of size %zu", (int) packet_type, (size_t) nr);
   //msg_f("Contents: %.*s", (int) nr, buf);

   if (! conn->handshake_received && packet_type != Packet_Type::Handshake_Response)
   {
      msg_f("Bad packet from server: expected handshake packet but got: %s",
            packet_type_string(packet_type));
      return false;
   }

   if (conn->handshake_received && packet_type == Packet_Type::Handshake_Response)
   {
      msg_f("Bad packet from server: unexpected handshake packet");
      return false;
   }

   if (! conn->requested_stream)
   {
      if (conn->requested_msn)
      {
         if (packet_type != Packet_Type::Send_Message_Range)
         {
            msg_f("Bad packet from server: expected message range but got: %s",
                  packet_type_string(packet_type));
            return false;
         }
      }

      if (! conn->requested_msn || conn->startmsn.has_value())
      {
         if (packet_type == Packet_Type::Send_Message_Range)
         {
            msg_f("Bad packet from server: Unexpected packet type, got: %s",
                  packet_type_string(packet_type));
            return false;
         }
      }
   }

   switch (packet_type)
   {
   case Packet_Type::Handshake_Response:
   {
      ssize_t expected_bytes = 18;
      //msg_f("Received handshake");
      if (nr != expected_bytes)
      {
         msg_f("Unexpected size of handshake packet. Expected %d, got: %d",
               (int) expected_bytes, (int) nr);
         return false;
      }
      conn->server_version_major = *(uint16_t *) (buf + 8);
      conn->server_version_minor = *(uint16_t *) (buf + 10);
      conn->meta_id = *(uint32_t *) (buf + 12);
      conn->meta_mirror_id = *(uint16_t *) (buf + 16);

      if (conn->server_version_major != 2 || conn->server_version_minor != 0)
      {
         msg_f("Unexpected version sent by server: %d.%d\n",
               conn->server_version_major, conn->server_version_minor);
         return false;
      }

      conn->handshake_received = true;
      return true;
   }
   case Packet_Type::Send_Message_Range:
   {
      if (nr != 24)
      {
         msg_f("Bad packet!");
         return false;
      }
      uint64_t msn = *(uint64_t *) (buf + 8);
      uint64_t msn_oldest = *(uint64_t *) (buf + 16);
      conn->startmsn.emplace(msn);
      msg_f("Meta ID: %" PRIu32 ", Meta Mirror ID: %" PRIu16 ", Newest MSN: %" PRIu64 ", oldest MSN: %" PRIu64,
         conn->meta_id, conn->meta_mirror_id, msn, msn_oldest);
      conn->curmsn = msn;
      //msg_f("Received message range! %" PRIu64, conn->startmsn.value());
      return true;
   }
   case Packet_Type::Send_Message:
   {
      //msg_f("Received message!");
      break;
   }
   case Packet_Type::Send_Close:
   {
      //msg_f("Connection terminated by server (TODO: decode reason!)");
      /*
      case Packet_Type::Send_Message_Stream_Crashed:
      {
         msg_f("Server reports internal error!");
         return false;
      }
      */
      return false;
      break;
   }
   default:
   {
      msg_f("Unexpected packet type %d", (int) packet_type);
      return false;
   }
   }

   switch (conn->options.report_type)
   {
      case Report_Type_Print_Message:
      {
         for (int i = 0; i < (int) nr; i++)
         {
            if ((unsigned) buf[i] < 32
                  || (unsigned) buf[i] >= 127)
               buf[i] = '.';
         }
         msg_f("Got msg %" PRIu64 " (size %zd): %.*s",
               conn->curmsn, nr, (int) nr, buf);
      }
      break;
      case Report_Type_Interactive_Count:
      {
         report_count(conn);
      }
      break;
      default:
      {
         if (conn->curmsn % 1024 == 0)
         {
            msg_f("msg: %" PRIu64 ", size: %d", conn->curmsn, (int) nr);
         }
      }
      break;
   }

   conn->nmsgs++;
   conn->curmsn++;

   if (conn->options.nmsgs.has_value())
   {
      if (conn->nmsgs == conn->options.nmsgs.value())
         return false;
   }

   return true;
}

class Arg_Reader
{
   int argc = 0;
   const char **argv = nullptr;
   int index = 0;
public:
   bool eof() const
   {
      return index == argc;
   }
   const char *get() const
   {
      assert(! eof());
      return argv[index];
   }
   void consume()
   {
      assert(! eof());
      ++ index;
   }
   bool parse_u64(uint64_t *value)
   {
      if (eof())
      {
         msg_f("ERROR: End of command-line arguments reached while expecting 64-bit unsigned number");
         return false;
      }
      const char *arg = get();
      if (sscanf(arg, "%" SCNu64, value) != 1)
      {
         msg_f("ERROR: Expected 64-bit unsigned number at command-line position %d. Got: %s",
               index, arg);
         return false;
      }
      consume();
      return true;
   }
   Arg_Reader(int argc, const char **argv)
      : argc(argc), argv(argv), index(0)
   {
   }
};

static bool parse_options(int argc, const char **argv, FileEventReceiverOptions *options)
{
   Arg_Reader arg_reader(argc, argv);

   if (arg_reader.eof())
      return false;

   options->unix_socket_path = arg_reader.get();
   arg_reader.consume();

   while (! arg_reader.eof())
   {
      const char *arg = arg_reader.get();

      if (! strcmp(arg, "-print"))
      {
         options->report_type = Report_Type_Print_Message;
      }
      else if (! strcmp(arg, "-count"))
      {
         options->report_type = Report_Type_Interactive_Count;
      }
      else if (! strcmp(arg, "-startmsn"))
      {
         arg_reader.consume();
         uint64_t value;
         if (! arg_reader.parse_u64(&value))
            return false;
         options->startmsn.emplace(value);
      }
      else if (! strcmp(arg, "-nmsgs"))
      {
         arg_reader.consume();
         uint64_t value;
         if (! arg_reader.parse_u64(&value))
            return false;
         options->nmsgs.emplace(value);
      }
      else
      {
         msg_f("Invalid arg: '%s'", arg);
         return false;
      }
   }
   return true;
}


struct FileEventReceiverNewProtocol
{
   int server_sock = -1;
   int client_sock = -1;
   unsigned conn_id = 0;
   Conn_State conn;
};


bool receive_event(FileEventReceiverNewProtocol *r)
{
   Conn_State *conn = &r->conn;
   for (;;)
   {
      uint64_t n = conn->nmsgs;

      while (conn->nmsgs == n)
      {
         if (! do_message(conn))
         {
            return false;
         }
      }

      // we have another event
      return true;
   }
}

Read_Event get_event(FileEventReceiverNewProtocol *r)
{
   Packet_Buffer *packet = &r->conn.receive_packet;
   Read_Event out;
   // 8 byte packet header (type Send_Message)
   // Send_Message packet:
   //   8 byte msn
   //   2 byte message size (should be removed)
   int skip_bytes = 8 + 8 + 2;
   out.buffer = packet->data + skip_bytes;
   out.size = packet->size - skip_bytes;
   return out;
}




static bool initFileEventReceiver(FileEventReceiverNewProtocol *r, FileEventReceiverOptions const& options)
{
   // Initialize server socket / connection
   {
      struct sockaddr_un sun = {};
      sun.sun_family = AF_UNIX;
      if (snprintf(sun.sun_path, sizeof sun.sun_path, "%s", options.unix_socket_path)
            > (int) sizeof sun.sun_path)
      {
         fatal_f("Socket Path is too long!");
      }

      // In case there is another socket at that place (likely from an earlier
      // run) we remove that rudely, without asking. This avoids EADDRINUSE
      // error.  This might fail because there is no such socket, or because of
      // other error like EPERM. We're not checking.
      unlink(options.unix_socket_path);

      r->server_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

      if (r->server_sock == -1)
      {
         fatal_f("Failed to socket(): %s", strerror(errno));
      }

      if (bind(r->server_sock, (struct sockaddr *) &sun, sizeof sun) == -1)
      {
         fatal_f("Failed to bind() server_sock: %s", strerror(errno));
      }

      if (listen(r->server_sock, 1) == -1)
      {
         fatal_f("Failed to listen() server_sock: %s", strerror(errno));
      }
   }

   // Initialize client socket / connection. We could make this repeatable
   {
      r->client_sock = accept(r->server_sock, NULL, NULL);

      if (r->client_sock == -1)
      {
         fatal_f("Failed to accept() connection from server_sock: %s",
               strerror(errno));
         return false;
      }

      unsigned conn_id = r->conn_id++;
      msg_f("Connection %u accepted", conn_id);

      Conn_State conn;
      conn.conn_id = conn_id;
      conn.client_sock = r->client_sock;
      conn.options = options;
      conn.last_time = get_thread_time();
      conn.startmsn = options.startmsn;

      r->conn = conn;
   }

   return true;
}

static void terminateFileEventReceiver(FileEventReceiverNewProtocol *r)
{
   close(r->server_sock);
   r->server_sock = -1;

   close(r->client_sock);
   r->client_sock = -1;
}

FileEventReceiverNewProtocol *FileEventReceiverNewProtocolCreate(int argc, const char **argv)
{
   FileEventReceiverOptions options;

   if (! parse_options(argc, argv, &options))
   {
      //msg_f("Usage: ./seqpacket-reader <unix-socket-path> [-startmsn <MSN>] [-print]");
      msg_f("Failed to parse options for FileEventReceiver. Syntax: <unix-socket-path> [-startmsn <MSN>]");
      return nullptr;
   }

   FileEventReceiverNewProtocol *r = new FileEventReceiverNewProtocol();

   if (! initFileEventReceiver(r, options))
   {
      delete r;
      return nullptr;
   }

   return r;
}

void FileEventReceiverNewProtocolDestroy(FileEventReceiverNewProtocol *r)
{
   terminateFileEventReceiver(r);
   delete r;
}

}

#ifdef SEQPACKET_READER_WITH_MAIN
int main(int argc, const char **argv)
{
   FileEventReceiverOptions options;

   if (! parse_options(argc - 1, argv + 1, &options))
   {
      msg_f("Usage: ./seqpacket-reader <unix-socket-path> [-startmsn <MSN>] [-print]");
      return 1;
   }

   struct sockaddr_un sun = {};
   sun.sun_family = AF_UNIX;
   if (snprintf(sun.sun_path, sizeof sun.sun_path, "%s", options.unix_socket_path)
         > (int) sizeof sun.sun_path)
   {
      fatal_f("Socket Path is too long!");
   }

   int server_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

   if (server_sock == -1)
   {
      fatal_f("Failed to socket(): %s", strerror(errno));
   }

   if (bind(server_sock, (struct sockaddr *) &sun, sizeof sun) == -1)
   {
      fatal_f("Failed to bind() server_sock: %s", strerror(errno));
   }

   if (listen(server_sock, 1) == -1)
   {
      fatal_f("Failed to listen() server_sock: %s", strerror(errno));
   }

   for (unsigned conn_id = 0; ; ++ conn_id)
   {
      int client_sock = accept(server_sock, NULL, NULL);

      if (client_sock == -1)
      {
         fatal_f("Failed to accept() connection from server_sock: %s",
               strerror(errno));
      }

      msg_f("Connection %u accepted", conn_id);

      Conn_State conn;
      conn.conn_id = conn_id;
      conn.client_sock = client_sock;
      conn.options = options;
      conn.last_time = get_thread_time();
      conn.startmsn = options.startmsn;

      for (;;)
      {
         if (! do_message(&conn))
            break;
      }
      report_count(&conn);

      msg_f("Received %" PRIu64 " msgs total", conn.nmsgs);

      close(client_sock);

      break; // !!! do we still need a loop here?
   }

   return 0;
}
#endif
