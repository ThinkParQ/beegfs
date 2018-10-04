#include "GetNodeInfoMsgEx.h"
#include <program/Program.h>

bool GetNodeInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetNodeInfoMsg incoming");

   GeneralNodeInfo info;

   App *app = Program::getApp();

   getCPUInfo(&(info.cpuName), &(info.cpuSpeed), &(info.cpuCount));
   getMemInfo(&(info.memTotal), &(info.memFree));
   info.logFilePath = app->getConfig()->getLogStdFile();

   Node& localNode = app->getLocalNode();
   NumNodeID nodeNumID = localNode.getNumID();

   GetNodeInfoRespMsg getNodeInfoRespMsg(localNode.getID(), nodeNumID, info);
   ctx.sendResponse(getNodeInfoRespMsg);

   LOG_DEBUG_CONTEXT(log, Log_SPAM, std::string("Sent a message with type: ") +
      StringTk::uintToStr(getNodeInfoRespMsg.getMsgType()) + std::string(" to admon"));

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), StorageOpCounter_GETNODEINFO,
      getMsgHeaderUserID() );

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
   // char buf[5000];
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
