#include "GetNodeInfoMsgEx.h"
#include <program/Program.h>

bool GetNodeInfoMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
   size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("GetNodeInfoMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a GetNodeInfoMsg") + peer);

   GeneralNodeInfo info;

   App *app = Program::getApp();

   getCPUInfo(&(info.cpuName), &(info.cpuSpeed), &(info.cpuCount));
   getMemInfo(&(info.memTotal), &(info.memFree));
   info.logFilePath = app->getConfig()->getLogStdFile();

   Node *localNode = app->getLocalNode();
   std::string nodeID = localNode->getID();
   uint16_t nodeNumID = localNode->getNumID();

   GetNodeInfoRespMsg getNodeInfoRespMsg(nodeID.c_str(), nodeNumID, info);
   getNodeInfoRespMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, getNodeInfoRespMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   LOG_DEBUG_CONTEXT(log, 5, std::string("Sent a message with type: " ) + StringTk::uintToStr(
         getNodeInfoRespMsg.getMsgType() ) + std::string(" to admon"));

   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_GETNODEINFO);

   return true;
}

void GetNodeInfoMsgEx::getMemInfo(int *memTotal, int *memFree)
{
   // MEGABYTE
   *memTotal = 0;
   *memFree = 0;

   std::ifstream info("/proc/meminfo");
   std::string buffer;

   while (info.good())
   {
      getline(info, buffer, '\n');

      if(buffer.find("MemTotal") != std::string::npos)
      {
         *memTotal = StringTk::strToInt(buffer.substr(buffer.find(":") + 1, buffer.length()))
            / 1000;
      }

      if(buffer.find("MemFree") != std::string::npos)
      {
         *memFree = StringTk::strToInt(buffer.substr(buffer.find(":") + 1, buffer.length())) / 1000;
      }
   }

   info.close();
}

void GetNodeInfoMsgEx::getCPUInfo(std::string *cpuName, int *cpuSpeed, int *cpuCount)
{
   *cpuName = "";
   // MHz
   *cpuSpeed = 0;
   *cpuCount = 0;

   // get cpuname
   //	char buf[5000];
   std::ifstream info("/proc/cpuinfo");
   std::string buffer;

   while (info.good())
   {
      getline(info, buffer, '\n');
      if(buffer.find("model name") != std::string::npos)
      {
         *cpuName = StringTk::trim(buffer.substr(buffer.find(":") + 1, buffer.length()));
      }

      if(buffer.find("cpu MHz") != std::string::npos)
      {
         *cpuSpeed = StringTk::strToInt(buffer.substr(buffer.find(":") + 1, buffer.length()));
      }

      if(buffer.find("processor") != std::string::npos)
      {
         (*cpuCount)++;
      }
   }

   info.close();
}
