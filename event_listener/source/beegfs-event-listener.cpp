#include <iostream>
#include <sstream>
#include <signal.h>

#include "beegfs/beegfs_file_event_log.hpp"

/* The client needs a list of events to be logged in the config file
 * (beegfs-client.conf):
 * sysFileEventLogMask = flush,trunc,setattr,close,link-op,read
 * (Or any combination of the above)
 */


/**
 * @brief Trivial JSON writer
 */
class JsonObject {

   public:

      template<typename T>
      JsonObject& keyValue(const std::string& key, const T& value) {
         if(!isFirstItem) {
            stream << ", ";

         } else {
            isFirstItem = false;
         }

         printValue(key);
         stream << ": ";
         printValue(value);
         return *this;
      }

      operator std::string() const {
         return str();
      }

      std::string str() const
      {
         return "{ " + stream.str() + " }";
      }

   protected:

      std::stringstream stream;
      bool isFirstItem=true;

      template<typename T>
      void printValue(const T& value) {
         stream << value;
      }

      void printValue(const std::string& value) {
         stream << "\"";
         writeEscaped(stream, value);
         stream << "\"";
      }

      void printValue(const char* value) {
         printValue(std::string(value));
      }

      void printValue(const JsonObject& json) {
         stream << json.str();
      }

      void writeEscaped(std::ostream& stream, const std::string& s)
      {
         for(const auto& x: s)
         {
            switch (x) {
               case 0x08:
                  stream << "\\b";
                  break;
               case 0x0c:
                  stream << "\\f";
                  break;
               case '\n':
                  stream << "\\n";
                  break;
               case '\\':
                  stream << "\\\\";
                  break;
               case '\t':
                  stream << "\\t";
                  break;
               case '\r':
                  stream << "\\r";
                  break;
               case '\"':
                  stream << "\\\"";
                  break;
               case '/':
                  stream << "\\/";
                  break;
               default:
                  stream << x;
            }
         }
      }

};

std::ostream& operator<<(std::ostream& os, const BeeGFS::packet& p)
{
   JsonObject json;
   json
      .keyValue("VersionMajor",   p.formatVersionMajor)
      .keyValue("VersionMinor",   p.formatVersionMinor)
      .keyValue("PacketSize",     p.size)
      .keyValue("Dropped",        p.droppedSeq)
      .keyValue("Missed",         p.missedSeq)
      .keyValue("Event",
         JsonObject()
            .keyValue("Type",           to_string(p.type))
            .keyValue("Path",           p.path)
            .keyValue("EntryId",        p.entryId)
            .keyValue("ParentEntryId",  p.parentEntryId)
            .keyValue("TargetPath",     p.targetPath)
            .keyValue("TargetParentId", p.targetParentId)
         );

   os << json.str();
   return os;
}

void shutdown(int)
{
  exit(0);
}

int main(const int argc, const char** argv)
{
   if (argc < 2) {
      std::cout << "BeeGFS File Event Listener\n"
                  "MODE ARGUMENTS:\n"
                  " Mandatory:\n"
                  " <fileEventLogTarget: unix socket file path>\n"
                  "\n"
                  " Optional:\n"
                  " <clientAddr: IP-Address of the beegfs client which is running hive update>\n"
                  " <updatePort: port number to connect with hive update>\n"
                  " <BEE_UPDATE_DEBUG: to enable/disable debug mode by giving 1/0>\n"
                  "\n"
                  "Usage:\n"
                  "  beegfs-event-listener <socket>\n\n"
                  "  The medatada server has to be pointed to the socket, so that it knows where to\n"
                  "  send the event log. Set\n"
                  "     sysFileEventLogTarget = unix://<path>\n"
                  "  in beegfs-meta.conf.\n"
                  "  The clients need a list of events to be logged in the config file (beegfs-client.conf):\n"
                  "    sysFileEventLogMask = flush,trunc,setattr,close,link-op,read\n"
                  "  Example:\n"
                  "     To use beegfs-event-listener without hive-index update service\n"
                  "     Enter the following line in beegfs-meta.conf:\n"
                  "       sysFileEventLogTarget = unix:///tmp/beegfslog\n"
                  "     and run \n"
                  "     beegfs-event-listener /tmp/beegfslog\n"
                  "     OR give unix socket file path (/tmp/beegfslog) in beegfs-eventlistener.conf file and run \n"
                  "     systemctl start beegfs-eventlistener\n"
                  "\n"
                  "     To use beegfs-eventlistener service with hive-index serivce to update index dir.\n"
                  "     Set all below parameters in the beegfs-eventlistener.conf file\n"
                  "     <fileEventLogTarget: unix socket file path>\n"
                  "     <clientAddr: IP-Address of the beegfs client which is running hive update>\n"
                  "     <updatePort: port number to connect with hive update>\n"
                  "     <BEE_UPDATE_DEBUG: to enable/disable debug mode by giving 1/0>\n"
                  "   Example:\n"
                  "   clientAddr=xxx.xxx.xxx.xxx\n"
                  "   fileEventLogTarget = /tmp/beegfslog\n"
                  "   updatePort=9000\n"
                  "   BEE_UPDATE_DEBUG=1\n"
                  " Note:\n"
                  " When hive-index's update service is successfully started, then only start beegfs-eventlistener serivce from all the meta nodes\n"

               << std::endl;

      return EXIT_FAILURE;
   }

    signal(SIGINT,  shutdown);

    BeeGFS::FileEventReceiver receiver(argv[1]);
    bool hiveUpdate = false;
    bool debug = false;
    if (argc > 2)
    {
       hiveUpdate = true;
       char *value = getenv("BEE_UPDATE_DEBUG");
       if (value)
       {
           if (strcmp(value, "1") == 0)
              debug = true;
       }
    }

    std::cout << JsonObject().keyValue("EventListener",
                           JsonObject().keyValue("Socket", argv[1])
                                       .keyValue("VersionMajor", BEEGFS_EVENTLOG_FORMAT_MAJOR)
                                       .keyValue("VersionMinor", BEEGFS_EVENTLOG_FORMAT_MINOR)
                  ).str() << std::endl;

    BeeGFS::BDMClient bdmClient;

    int port = BEEGFS_EVENTLOG_BDM_PORT;

    if (hiveUpdate)
    {
       bool connected;
       std::cout << "Sending the event logs to BeeGFS hive update service" << std::endl;

       if (argc > 3)
          port = atoi(argv[3]);

       do
       {
          connected = bdmClient.connToBDM(std::string(argv[2]), port);
          if (!connected)
          {
              std::cerr << "Failed to connect with BeeGFS hive update service, retry" << std::endl;
              sleep(1);
          }
       } while (!connected);
       std::cout << "Successfully connected with BeeGFS hive update service" << std::endl;
    }

    while (true)
    {
       using BeeGFS::FileEventReceiver;

       if (hiveUpdate)
       {
          auto serialData = receiver.readSerializedData();

          if (serialData.empty())
          {
             std::cerr << "Reading serialized data failed" << std::endl;
             continue;
          }


          if (debug)
          {
             // Convert the serialized data to packet for printing
             auto result = receiver.rawToPacket(serialData.data(),
                                                serialData.size());
             std::cout << result.second << std::endl;
          }

          int ret = bdmClient.sendData(serialData.data(), serialData.size());
          if (ret)
          {
             std::cerr << "Failed to send data" << std::endl;
             std::cerr << serialData.data() << std::endl;
          }
       }
       else
       {
          const auto data = receiver.read();

          switch (data.first) {
          case FileEventReceiver::ReadErrorCode::Success:
             std::cout << data.second << std::endl;
             break;
          case FileEventReceiver::ReadErrorCode::VersionMismatch:
             std::cerr << "Invalid Packet Version" << std::endl;
             break;
          case FileEventReceiver::ReadErrorCode::InvalidSize:
             std::cerr << "Invalid Packet Size" << std::endl;
             break;
          case FileEventReceiver::ReadErrorCode::ReadFailed:
             std::cerr << "Read Failed" << std::endl;
             break;
          }
       }
    }

    std::cout << "Exit listener" << std::endl;
    return 0;
}
