#include <common/app/AbstractApp.h>
#include <common/system/System.h>
#include <common/threading/PThread.h>
#include <common/toolkit/ZipIterator.h>
#include "MsgHelperGenericDebug.h"


#define GENDBGMSG_TXTFILE_MAX_READ_LEN    (100*1024)


std::string MsgHelperGenericDebug::processOpVarLogMessages(std::istringstream& commandStream)
{
   return loadTextFile("/var/log/messages");
}

std::string MsgHelperGenericDebug::processOpVarLogKernLog(std::istringstream& commandStream)
{
   return loadTextFile("/var/log/kern.log");
}

std::string MsgHelperGenericDebug::processOpFhgfsLog(std::istringstream& commandStream)
{
   auto cfg = PThread::getCurrentThreadApp()->getCommonConfig();

   if(cfg->getLogStdFile().empty() )
      return "No log file defined.";

   return loadTextFile(cfg->getLogStdFile() );
}

std::string MsgHelperGenericDebug::processOpLoadAvg(std::istringstream& commandStream)
{
   return loadTextFile("/proc/loadavg");
}

std::string MsgHelperGenericDebug::processOpDropCaches(std::istringstream& commandStream)
{
   return writeTextFile("/proc/sys/vm/drop_caches", "3");
}

std::string MsgHelperGenericDebug::processOpGetLogLevel(std::istringstream& commandStream)
{
   // procotol: no arguments

   Logger* log = Logger::getLogger();
   std::ostringstream responseStream;
   std::string logTopicStr;

   const IntVector logLevels = log->getLogLevels();

   for (size_t i = 0; i < logLevels.size(); i++)
      responseStream << Logger::logTopicToName((LogTopic)i) << ": " << logLevels[i] << std::endl;

   return responseStream.str();
}

std::string MsgHelperGenericDebug::processOpSetLogLevel(std::istringstream& commandStream)
{
   // procotol: "newLogLevel logTopicName"

   Logger* log = Logger::getLogger();
   std::ostringstream responseStream;

   std::string logLevelStr;
   std::string logTopicStr;

   // get logLevel from command string
   std::getline(commandStream, logLevelStr, ' ');

   if(logLevelStr.empty() )
      return "Invalid or missing logLevel";

   int logLevel = StringTk::strToInt(logLevelStr);

   // get logTopic from command string
   std::getline(commandStream, logTopicStr, ' ');

   LogTopic logTopic;

   if (!logTopicStr.empty())
   {
      logTopic = Logger::logTopicFromName(logTopicStr);

      if (logTopic == LogTopic_INVALID)
         return "Log topic " + logTopicStr + " doesn't exist.";

      responseStream << "Log topic: " << logTopicStr << ";"
                     << " Old level: " << log->getLogLevel(logTopic) << ";";

      log->setLogLevel(logLevel, logTopic);

      responseStream << " New level: " << log->getLogLevel(logTopic) << std::endl;
   }
   else
   { // no log topic given => set log level for all topics
      const IntVector logLevels = log->getLogLevels();

      for (size_t i = 0; i < logLevels.size(); i++)
      {
         responseStream << "Log topic: " << Logger::logTopicToName((LogTopic)i) << ";"
                        << " Old level: " << log->getLogLevel((LogTopic)i) << ";";

         log->setLogLevel(logLevel, (LogTopic)i);

         responseStream << " New level: " << log->getLogLevel((LogTopic)i) << std::endl;
      }
   }
   return responseStream.str();
}

/**
 * @param cfgFile may be an empty string, in which case an error message will be returned.
 */
std::string MsgHelperGenericDebug::processOpCfgFile(std::istringstream& commandStream,
   std::string cfgFile)
{
   if(cfgFile.empty() )
      return "No config file defined.";

   return loadTextFile(cfgFile);
}

/**
 * Print currently established (outgoing) connections of this node to other nodes, similar to
 * the output of the "fhgfs-net" tool.
 */
std::string MsgHelperGenericDebug::processOpNetOut(std::istringstream& commandStream,
   const NodeStoreServers* mgmtNodes, const NodeStoreServers* metaNodes,
   const NodeStoreServers* storageNodes)
{
   std::ostringstream responseStream;

   responseStream << printNodeStoreConns(mgmtNodes, "mgmt_nodes") << std::endl;
   responseStream << printNodeStoreConns(metaNodes, "meta_nodes") << std::endl;
   responseStream << printNodeStoreConns(storageNodes, "storage_nodes") << std::endl;

   return responseStream.str();
}

/**
 * Opens a text file and reads it. Only the last part (see GENDBGMSG_TXTFILE_MAX_READ_LEN) is
 * returned if the file is too large.
 *
 * @return file contents or human-readable error string will be returned if file cannot be read.
 */
std::string MsgHelperGenericDebug::loadTextFile(std::string path)
{
   const size_t maxLineLen = 2048;
   char line[maxLineLen];
   std::ostringstream responseStream;
   size_t bytesRead = 0;

   struct stat statBuf;

   std::ifstream fis(path.c_str() );
   if(!fis.is_open() || fis.fail() )
      return "Unable to open file: " + path + ". (SysErr: " + System::getErrString() + ")";

   // seek if file too large

   int statRes = stat(path.c_str(), &statBuf);
   if(statRes != 0)
      return "Unable to stat file. SysErr: " + System::getErrString();


   if(statBuf.st_size > GENDBGMSG_TXTFILE_MAX_READ_LEN)
      fis.seekg(statBuf.st_size - GENDBGMSG_TXTFILE_MAX_READ_LEN);


   while(!fis.eof() )
   {
      line[0] = 0; // make sure line is zero-terminated in case of read error

      fis.getline(line, maxLineLen);

      if(!fis.eof() && fis.fail() )
      {
         responseStream << "File read error occurred. SysErr: " + System::getErrString();
         break;
      }

      // the file might grow while we're reading it, so we check the amount of read data again below
      size_t lineLen = strlen(line);
      if( (lineLen + bytesRead + 1) > GENDBGMSG_TXTFILE_MAX_READ_LEN) // (+1 for newline)
         break;

      bytesRead += lineLen + 1; // (+1 for newline)

      responseStream << line << std::endl;
   }

   fis.close();

   return responseStream.str();
}


/**
 * Opens a file and writes text to it. Will try to create the file if it doesn't exist yet. File
 * will be truncated if it exists already.
 *
 * @param writeStr: The string that should be written to the file
 * @return: human-readable error string will be returned if file cannot be read.
 */
std::string MsgHelperGenericDebug::writeTextFile(std::string path, std::string writeStr)
{
   std::ofstream fos(path.c_str() );
   if(!fos.is_open() || fos.fail() )
      return "Unable to open file. SysErr: " + System::getErrString();

   fos << writeStr;

   if(fos.fail() )
      return "Error during file write. SysErr: " + System::getErrString();

   fos.close();

   // (note: errno doesn't work after close, so we cannot write the corresponding errString here.)

   if(fos.fail() )
      return "Error occurred during file close.";

   return "Completed successfully";
}

/**
 * Print node name and currently established connections for each node in given store, similar to
 * the "fhgfs-net" tool.
 */
std::string MsgHelperGenericDebug::printNodeStoreConns(const NodeStoreServers* nodes,
   std::string headline)
{
   std::ostringstream returnStream;

   returnStream << headline << std::endl;

   returnStream << std::string(headline.length(), '=') << std::endl;

   for (const auto& node : nodes->referenceAllNodes())
   {
      returnStream << node->getID() << " [ID: " << node->getNumID() << "]" << std::endl;

      returnStream << printNodeConns(*node);
   }

   return returnStream.str();
}

/**
 * Print currently established connections to the given node, similar to the corresponding output
 * lines of the "fhgfs-net" tool.
 */
std::string MsgHelperGenericDebug::printNodeConns(Node& node)
{
   std::ostringstream returnStream;

   NodeConnPool* connPool = node.getConnPool();
   NodeConnPoolStats poolStats;

   const char* nonPrimaryTag = " [fallback route]"; // extra text to hint at non-primary conns
   bool isNonPrimaryConn;


   returnStream << "   Connections: ";

   connPool->getStats(&poolStats);

   if(!(poolStats.numEstablishedStd +
        poolStats.numEstablishedSDP +
        poolStats.numEstablishedRDMA) )
      returnStream << "<none>";
   else
   { // print "<type>: <count> (<first_avail_peername>);"
      if(poolStats.numEstablishedStd)
      {
         std::string peerName;

         connPool->getFirstPeerName(NICADDRTYPE_STANDARD, &peerName,
            &isNonPrimaryConn);

         returnStream << "TCP: " << poolStats.numEstablishedStd << " (" << peerName <<
            (isNonPrimaryConn ? nonPrimaryTag : "") << "); ";
      }

      if(poolStats.numEstablishedSDP)
      {
         std::string peerName;

         connPool->getFirstPeerName(NICADDRTYPE_SDP, &peerName,
            &isNonPrimaryConn);

         returnStream << "SDP: " << poolStats.numEstablishedSDP << " (" << peerName <<
            (isNonPrimaryConn ? nonPrimaryTag : "") << "); ";
      }

      if(poolStats.numEstablishedRDMA)
      {
         std::string peerName;

         connPool->getFirstPeerName(NICADDRTYPE_RDMA, &peerName,
            &isNonPrimaryConn);

         returnStream << "RDMA: " << poolStats.numEstablishedRDMA << " (" << peerName <<
            (isNonPrimaryConn ? nonPrimaryTag : "") << "); ";
      }
   }

   returnStream << std::endl;

   return returnStream.str();
}

/**
 * Prints IDs with currently exceeded quota. Valid daemon types are the management daemon and the
 * storage daemon. This debug command requires the parameters for QuotaDataType (uid/gid) and
 * QuotaLimitType (size/inode).
 *
 * @param commandStream the stream with the command line parameters
 * @param store the store with the exceeded quota
 *
 * @return the string output with the exceeded IDs
 */
std::string MsgHelperGenericDebug::processOpQuotaExceeded(std::istringstream& commandStream,
   const ExceededQuotaStore* store)
{
   if (!store)
      return "Invalid storage pool or target ID.";

   QuotaDataType quotaDataType = QuotaDataType_NONE;
   std::string quotaDataTypeStr;

   QuotaLimitType quotaLimitType = QuotaLimitType_NONE;
   std::string quotaLimitTypeStr;

   // get parameter from command string
   std::string inputString;
   while(!commandStream.eof() )
   {
      std::getline(commandStream, inputString, ' ');

      if(inputString == "uid")
      {
         quotaDataType = QuotaDataType_USER;
         quotaDataTypeStr = "User";
      }
      else
      if(inputString == "gid")
      {
         quotaDataType = QuotaDataType_GROUP;
         quotaDataTypeStr = "Group";
      }
      else
      if(inputString == "size")
      {
         quotaLimitType = QuotaLimitType_SIZE;
         quotaLimitTypeStr = "file size";
      }
      else
      if(inputString == "inode")
      {
         quotaLimitType = QuotaLimitType_INODE;
         quotaLimitTypeStr = "inode";
      }
   }

   // verify given parameters
   if(quotaDataType == QuotaDataType_NONE)
      return "Invalid or missing quota data type argument.";
   if(quotaLimitType == QuotaLimitType_NONE)
      return "Invalid or missing quota limit type argument.";


   // create the string for the response message
   std::ostringstream returnStream;
   returnStream << quotaDataTypeStr << " IDs with exceeded " << quotaLimitTypeStr << " quota:"
      << std::endl;

   UIntList idList;
   store->getExceededQuota(&idList, quotaDataType, quotaLimitType);

   for(UIntListIter iter = idList.begin(); iter != idList.end(); iter++)
      returnStream << *iter << std::endl;

   return returnStream.str();
}

/**
 * Print contents of given TargetStateStorage.
 */
std::string MsgHelperGenericDebug::processOpListTargetStates(std::istringstream& commandStream,
   const TargetStateStore* targetStateStore)
{
   // protocol: no arguments

   std::ostringstream returnStream;

   UInt16List targetIDs;
   UInt8List targetConsistencyStates;
   UInt8List targetReachabilityStates;

   targetStateStore->getStatesAsLists(targetIDs, targetReachabilityStates, targetConsistencyStates);

   for (ZipConstIterRange<UInt16List, UInt8List, UInt8List>
      iter(targetIDs, targetReachabilityStates, targetConsistencyStates);
      !iter.empty(); ++iter)
   {
      returnStream << *iter()->first << " "
            << TargetStateStore::stateToStr( (TargetReachabilityState)*iter()->second) << " "
            << TargetStateStore::stateToStr( (TargetConsistencyState)*iter()->third)
            << std::endl;
   }

   return returnStream.str();
}

std::string MsgHelperGenericDebug::processOpListStoragePools(std::istringstream& commandStream,
   const StoragePoolStore* storagePoolStore)
{
   // protocol: no arguments

   std::ostringstream returnStream;

   UInt16List targetIDs;
   UInt8List targetConsistencyStates;
   UInt8List targetReachabilityStates;

   StoragePoolPtrVec pools = storagePoolStore->getPoolsAsVec();

   for (StoragePoolPtrVecCIter it = pools.begin(); it != pools.end(); it++)
   {
      returnStream
         << (*it)->getId()
         << " : "
         << (*it)->getDescription()
         << std::endl;
   }

   return returnStream.str();
}
