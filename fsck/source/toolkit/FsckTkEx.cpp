#include "FsckTkEx.h"

#include <common/net/message/fsck/FsckSetEventLoggingMsg.h>
#include <common/net/message/fsck/FsckSetEventLoggingRespMsg.h>
#include <common/net/message/storage/StatStoragePathMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/toolkit/ListTk.h>
#include <common/toolkit/FsckTk.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>

#include <mutex>

char FsckTkEx::progressChar = '-';
Mutex FsckTkEx::outputMutex;

/*
 * check the reachability of all nodes in the system
 */
bool FsckTkEx::checkReachability()
{
   NodeStore* metaNodes = Program::getApp()->getMetaNodes();
   NodeStore* storageNodes = Program::getApp()->getStorageNodes();

   StringList errors;
   bool commSuccess = true;

   FsckTkEx::fsckOutput("Step 1: Check reachability of nodes: ", OutputOptions_FLUSH);

   if ( metaNodes->getSize() == 0 )
   {
      errors.push_back("No metadata nodes found");
      commSuccess = false;
   }

   if ( storageNodes->getSize() == 0 )
   {
      errors.push_back("No storage nodes found");
      commSuccess = false;
   }

   for (const auto& node : metaNodes->referenceAllNodes())
   {
      if ( !FsckTkEx::checkReachability(*node, NODETYPE_Meta) )
      {
         errors.push_back("Communication with metadata node failed: " + node->getID());
         commSuccess = false;
      }
   }

   for (const auto& node : storageNodes->referenceAllNodes())
   {
      if ( !FsckTkEx::checkReachability(*node, NODETYPE_Storage) )
      {
         errors.push_back("Communication with storage node failed: " + node->getID());
         commSuccess = false;
      }
   }

   if ( commSuccess )
      FsckTkEx::fsckOutput("Finished", OutputOptions_LINEBREAK);
   else
   {
      for ( StringListIter iter = errors.begin(); iter != errors.end(); iter++ )
      {
         FsckTkEx::fsckOutput(*iter,
            OutputOptions_NONE | OutputOptions_LINEBREAK | OutputOptions_STDERR);
      }
   }

   return commSuccess;
}

/*
 * check reachability of a single node
 */
bool FsckTkEx::checkReachability(Node& node, NodeType nodetype)
{
   bool retVal = false;
   HeartbeatRequestMsg heartbeatRequestMsg;
   std::string realNodeID = node.getID();

   const auto respMsg = MessagingTk::requestResponse(node, heartbeatRequestMsg,
         NETMSGTYPE_Heartbeat);
   if (respMsg)
   {
      HeartbeatMsg *heartbeatMsg = (HeartbeatMsg *) respMsg.get();
      std::string receivedNodeID = heartbeatMsg->getNodeID();
      retVal = receivedNodeID.compare(realNodeID) == 0;
   }

   return retVal;
}

void FsckTkEx::fsckOutput(std::string text, int optionFlags)
{
   const std::lock_guard<Mutex> lock(outputMutex);

   static bool fileErrLogged = false; // to make sure we print logfile open err only once

   Config* cfg = Program::getApp()->getConfig(); // might be NULL on app init failure
   bool toLog = cfg && (!(OutputOptions_NOLOG & optionFlags)); // true if write to log file

   FILE *logFile = NULL;

   if (likely(toLog))
   {
      std::string logFilePath = cfg->getLogOutFile();

      logFile = fopen(logFilePath.c_str(),"a+");
      if (logFile == NULL)
      {
         toLog = false;

         if(!fileErrLogged)
         {
            std::cerr << "Cannot open output file for writing: '" << logFilePath << "'"
               << std::endl;

            fileErrLogged = true;
         }
      }
   }

   const char* colorNormal = OutputColor_NORMAL;
   const char* color = OutputColor_NORMAL;

   FILE *outFile = stdout;

   if (OutputOptions_STDERR & optionFlags)
   {
      outFile = stderr;
   }
   else if (OutputOptions_NOSTDOUT & optionFlags)
   {
      outFile = NULL;
   }

   bool outFileIsTty;

   if (outFile)
      outFileIsTty = isatty(fileno(outFile));
   else
      outFileIsTty = false;

   if (OutputOptions_COLORGREEN & optionFlags)
   {
      color = OutputColor_GREEN;
   }
   else if (OutputOptions_COLORRED & optionFlags)
   {
      color = OutputColor_RED;
   }
   else
   {
      color = OutputColor_NORMAL;
   }

   if (OutputOptions_LINEDELETE & optionFlags)
   {
      optionFlags = optionFlags | OutputOptions_FLUSH;
      SAFE_FPRINTF(outFile, "\r");
      SAFE_FPRINTF(outFile, "                                                                   ");
      SAFE_FPRINTF(outFile, "\r");
   }

   if (OutputOptions_ADDLINEBREAKBEFORE & optionFlags)
   {
      SAFE_FPRINTF(outFile, "\n");
      if (likely(toLog)) SAFE_FPRINTF(logFile,"\n");
   }

   if (OutputOptions_HEADLINE & optionFlags)
   {
      SAFE_FPRINTF(outFile, "\n--------------------------------------------------------------------\n");

      if (likely(toLog))
         SAFE_FPRINTF(logFile,"\n--------------------------------------------------------------------\n");

      if (likely(outFileIsTty))
         SAFE_FPRINTF(outFile, "%s%s%s",color,text.c_str(),colorNormal);
      else
         SAFE_FPRINTF(outFile, "%s",text.c_str());

      if (likely(toLog))
         SAFE_FPRINTF(logFile,"%s",text.c_str());

      SAFE_FPRINTF(outFile, "\n--------------------------------------------------------------------\n") ;

      if (likely(toLog))
         SAFE_FPRINTF(logFile,"\n--------------------------------------------------------------------\n");
   }
   else
   {
      if (likely(outFileIsTty))
         SAFE_FPRINTF(outFile, "%s%s%s",color,text.c_str(),colorNormal);
      else
         SAFE_FPRINTF(outFile, "%s",text.c_str());

      if (likely(toLog)) SAFE_FPRINTF(logFile,"%s",text.c_str());
   }

   if (OutputOptions_LINEBREAK & optionFlags)
   {
      SAFE_FPRINTF(outFile, "\n");
      if (likely(toLog))
         SAFE_FPRINTF(logFile,"\n");
   }

   if (OutputOptions_DOUBLELINEBREAK & optionFlags)
   {
      SAFE_FPRINTF(outFile, "\n\n");
      if (likely(toLog))
         SAFE_FPRINTF(logFile,"\n\n");
   }

   if (OutputOptions_FLUSH & optionFlags)
   {
      fflush(outFile);
      if (likely(toLog))
         fflush(logFile);
   }

   if (logFile != NULL)
   {
      fclose(logFile);
   }
}

void FsckTkEx::printVersionHeader(bool toStdErr, bool noLogFile)
{
   int optionFlags = OutputOptions_LINEBREAK;
   if (toStdErr)
   {
      optionFlags = OutputOptions_LINEBREAK | OutputOptions_STDERR;
   }
   if (noLogFile)
   {
      optionFlags = optionFlags | OutputOptions_NOLOG;
   }

   FsckTkEx::fsckOutput("\n", optionFlags);
   FsckTkEx::fsckOutput("BeeGFS File System Check Version : " + std::string(BEEGFS_VERSION),
      optionFlags);
   FsckTkEx::fsckOutput("----", optionFlags);
}

void FsckTkEx::progressMeter()
{
   const std::lock_guard<Mutex> lock(outputMutex);

   printf("\b%c",progressChar);
   fflush(stdout);

   switch(progressChar)
   {
      case '-' :
      {
         progressChar = '\\';
         break;
      }
      case '\\' :
      {
         progressChar = '|';
         break;
      }
      case '|' :
      {
         progressChar = '/';
         break;
      }
      case '/' :
      {
         progressChar = '-';
         break;
      }
      default:
      {
         progressChar = '-';
         break;
      }

   }
}

/*
 * this is only a rough approximation
 */
int64_t FsckTkEx::calcNeededSpace()
{
   const char* logContext = "FsckTkEx (calcNeededSpace)";
   int64_t neededSpace = 0;

   // get used inodes from all meta data servers and sum them up
   NodeStore* metaNodes = Program::getApp()->getMetaNodes();

   for (const auto& metaNode : metaNodes->referenceAllNodes())
   {
      NumNodeID nodeID = metaNode->getNumID();

      StatStoragePathMsg statStoragePathMsg(0);

      const auto respMsg = MessagingTk::requestResponse(*metaNode, statStoragePathMsg,
         NETMSGTYPE_StatStoragePathResp);
      if (respMsg)
      {
         StatStoragePathRespMsg* statStoragePathRespMsg = (StatStoragePathRespMsg *) respMsg.get();
         int64_t usedInodes = statStoragePathRespMsg->getInodesTotal()
            - statStoragePathRespMsg->getInodesFree();
         neededSpace += usedInodes * NEEDED_DISKSPACE_META_INODE;
      }
      else
      {
         LogContext(logContext).logErr(
            "Unable to calculate needed disk space; Communication error with node: "
            + nodeID.str());
         return 0;
      }
   }

   // get used inodes from all storage servers and sum them up
   NodeStore* storageNodes = Program::getApp()->getStorageNodes();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();

   for (const auto& storageNode : storageNodes->referenceAllNodes())
   {
      NumNodeID nodeID = storageNode->getNumID();
      UInt16List targetIDs;
      targetMapper->getTargetsByNode(nodeID, targetIDs);

      for ( UInt16ListIter targetIDIter = targetIDs.begin(); targetIDIter != targetIDs.end();
         targetIDIter++ )
      {
         uint16_t targetID = *targetIDIter;
         StatStoragePathMsg statStoragePathMsg(targetID);

         const auto respMsg = MessagingTk::requestResponse(*storageNode, statStoragePathMsg,
            NETMSGTYPE_StatStoragePathResp);
         if (respMsg)
         {
            auto* statStoragePathRespMsg = (StatStoragePathRespMsg *) respMsg.get();
            int64_t usedInodes = statStoragePathRespMsg->getInodesTotal()
               - statStoragePathRespMsg->getInodesFree();
            neededSpace += usedInodes * NEEDED_DISKSPACE_STORAGE_INODE;
         }
         else
         {
            LogContext(logContext).logErr(
               "Unable to calculate needed disk space; Communication error with node: "
               + nodeID.str());
            return -1;
         }
      }
   }

   // now we take the calculated approximation and double it to have a lot of space for errors
   return neededSpace*2;
}

bool FsckTkEx::checkDiskSpace(Path& dbPath)
{
   int64_t neededDiskSpace = FsckTkEx::calcNeededSpace();

   if ( unlikely(neededDiskSpace < 0) )
   {
      FsckTkEx::fsckOutput("Could not determine needed disk space. Aborting now.",
         OutputOptions_LINEBREAK | OutputOptions_ADDLINEBREAKBEFORE);
      return false;
   }

   int64_t sizeTotal;
   int64_t sizeFree;
   int64_t inodesTotal;
   int64_t inodesFree;
   bool statRes = StorageTk::statStoragePath(dbPath, true, &sizeTotal, &sizeFree,
      &inodesTotal, &inodesFree);

   if (!statRes)
   {
      FsckTkEx::fsckOutput(
         "Could not stat database file path to determine free space; database file: "
            + dbPath.str());
      return false;
   }

   if ( neededDiskSpace >= sizeFree )
   {
      std::string neededDiskSpaceUnit;
      double neededDiskSpaceValue = UnitTk::byteToXbyte(neededDiskSpace, &neededDiskSpaceUnit);

      std::string sizeFreeUnit;
      double sizeFreeValue = UnitTk::byteToXbyte(sizeFree, &sizeFreeUnit);

      FsckTkEx::fsckOutput(
         "Not enough disk space to create database file: " + dbPath.str()
            + "; Recommended free space: " + StringTk::doubleToStr(neededDiskSpaceValue)
            + neededDiskSpaceUnit + "; Free space: " + StringTk::doubleToStr(sizeFreeValue)
            + sizeFreeUnit, OutputOptions_LINEBREAK | OutputOptions_ADDLINEBREAKBEFORE);

      bool ignoreDBDiskSpace = Program::getApp()->getConfig()->getIgnoreDBDiskSpace();
      if(!ignoreDBDiskSpace)
         return false;
   }

   return true;
}

std::string FsckTkEx::getRepairActionDesc(FsckRepairAction repairAction, bool shortDesc)
{
   for (size_t i = 0; __FsckRepairActions[i].actionDesc != nullptr; i++)
   {
      if( repairAction == __FsckRepairActions[i].action )
      { // we have a match
         if (shortDesc)
            return __FsckRepairActions[i].actionShortDesc;
         else
            return __FsckRepairActions[i].actionDesc;
      }
   }

   return "";
}

FhgfsOpsErr FsckTkEx::startModificationLogging(NodeStore* metaNodes, Node& localNode,
      bool forceRestart)
{
   const char* logContext = "FsckTkEx (startModificationLogging)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   NicAddressList localNicList = localNode.getNicList();
   unsigned localPortUDP = localNode.getPortUDP();

   FsckTkEx::fsckOutput("-----",
      OutputOptions_ADDLINEBREAKBEFORE | OutputOptions_FLUSH | OutputOptions_LINEBREAK);
   FsckTkEx::fsckOutput(
      "Waiting for metadata servers to start modification logging. This might take some time.",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);
   FsckTkEx::fsckOutput("-----", OutputOptions_FLUSH | OutputOptions_DOUBLELINEBREAK);

   NumNodeIDList nodeIDs;

   auto metaNodeList = metaNodes->referenceAllNodes();

   for (auto iter = metaNodeList.begin(); iter != metaNodeList.end(); iter++)
   {
      auto node = metaNodes->referenceNode((*iter)->getNumID());

      NicAddressList nicList;
      FsckSetEventLoggingMsg fsckSetEventLoggingMsg(true, localPortUDP,
         &localNicList, forceRestart);

      const auto respMsg = MessagingTk::requestResponse(*node, fsckSetEventLoggingMsg,
         NETMSGTYPE_FsckSetEventLoggingResp);

      if (respMsg)
      {
         auto* fsckSetEventLoggingRespMsg = (FsckSetEventLoggingRespMsg*) respMsg.get();

         bool started = fsckSetEventLoggingRespMsg->getLoggingEnabled();

         if (!started) // EventFlusher was already running on this node!
         {
            LogContext(logContext).logErr("Modification logging already running on node: "
                  + node->getID());

            retVal = FhgfsOpsErr_INUSE;
            break;
         }
      }
      else
      {
         LogContext(logContext).logErr("Communication error occured with node: " + node->getID());
         retVal = FhgfsOpsErr_COMMUNICATION;
      }
   }

   return retVal;
}

bool FsckTkEx::stopModificationLogging(NodeStore* metaNodes)
{
   const char* logContext = "FsckTkEx (stopModificationLogging)";

   bool retVal = true;

   FsckTkEx::fsckOutput("-----",
      OutputOptions_ADDLINEBREAKBEFORE | OutputOptions_FLUSH | OutputOptions_LINEBREAK);
   FsckTkEx::fsckOutput(
      "Waiting for metadata servers to stop modification logging. This might take some time.",
      OutputOptions_FLUSH | OutputOptions_LINEBREAK);
   FsckTkEx::fsckOutput("-----", OutputOptions_FLUSH | OutputOptions_DOUBLELINEBREAK);

   NumNodeIDList nodeIDs;

   auto metaNodeList = metaNodes->referenceAllNodes();

   for (auto iter = metaNodeList.begin(); iter != metaNodeList.end(); iter++)
      nodeIDs.push_back((*iter)->getNumID());

   NumNodeIDListIter nodeIdIter = nodeIDs.begin();

   while (! nodeIDs.empty())
   {
      NumNodeID nodeID = *nodeIdIter;

      auto node = metaNodes->referenceNode(nodeID);

      NicAddressList nicList;
      FsckSetEventLoggingMsg fsckSetEventLoggingMsg(false, 0, &nicList, false);

      const auto respMsg = MessagingTk::requestResponse(*node, fsckSetEventLoggingMsg,
         NETMSGTYPE_FsckSetEventLoggingResp);

      if (respMsg)
      {
         auto* fsckSetEventLoggingRespMsg = (FsckSetEventLoggingRespMsg*) respMsg.get();

         bool result = fsckSetEventLoggingRespMsg->getResult();
         bool missedEvents = fsckSetEventLoggingRespMsg->getMissedEvents();

         if ( result )
         {
            nodeIdIter = nodeIDs.erase(nodeIdIter);
            if ( missedEvents )
            {
               retVal = false;
            }
         }
         else
            nodeIdIter++; // keep in list and try again later
      }
      else
      {
         LogContext(logContext).logErr("Communication error occured with node: " + node->getID());
         retVal = false;
      }

      if (nodeIdIter == nodeIDs.end())
      {
         nodeIdIter = nodeIDs.begin();
         sleep(5);
      }
   }

   return retVal;
}

bool FsckTkEx::checkConsistencyStates()
{
   auto mgmtdNode = Program::getApp()->getMgmtNodes()->referenceFirstNode();
   if (!mgmtdNode)
      throw std::runtime_error("Management node not found");

   std::list<uint8_t> storageReachabilityStates;
   std::list<uint8_t> storageConsistencyStates;
   std::list<uint16_t> storageTargetIDs;
   std::list<uint8_t> metaReachabilityStates;
   std::list<uint8_t> metaConsistencyStates;
   std::list<uint16_t> metaTargetIDs;

   if (!NodesTk::downloadTargetStates(*mgmtdNode, NODETYPE_Storage, &storageTargetIDs,
           &storageReachabilityStates, &storageConsistencyStates, false)
         || !NodesTk::downloadTargetStates(*mgmtdNode, NODETYPE_Meta, &metaTargetIDs,
           &metaReachabilityStates, &metaConsistencyStates, false))
   {
      throw std::runtime_error("Could not download target states from management.");
   }


   bool result = true;

   {
      auto idIt = storageTargetIDs.begin();
      auto stateIt = storageConsistencyStates.begin();
      for (; idIt != storageTargetIDs.end() && stateIt != storageConsistencyStates.end();
            idIt++, stateIt++)
      {
         if (*stateIt == TargetConsistencyState_NEEDS_RESYNC)
         {
            FsckTkEx::fsckOutput("Storage target " + StringTk::uintToStr(*idIt) + " is set to "
                  "NEEDS_RESYNC.", OutputOptions_LINEBREAK);
            result = false;
         }
      }
   }

   {
      auto idIt = metaTargetIDs.begin();
      auto stateIt = metaConsistencyStates.begin();
      for (; idIt != metaTargetIDs.end() && stateIt != metaConsistencyStates.end(); 
            idIt++, stateIt++)
      {
         if (*stateIt == TargetConsistencyState_NEEDS_RESYNC)
         {
            FsckTkEx::fsckOutput("Meta node " + StringTk::uintToStr(*idIt) + " is set to "
                  "NEEDS_RESYNC.", OutputOptions_LINEBREAK);
            result = false;
         }
      }
   }

   return result;
}
