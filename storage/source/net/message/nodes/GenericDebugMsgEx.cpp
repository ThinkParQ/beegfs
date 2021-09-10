#include <common/net/message/nodes/GenericDebugRespMsg.h>
#include <common/net/msghelpers/MsgHelperGenericDebug.h>
#include <common/storage/quota/Quota.h>
#include <common/storage/StoragePoolId.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include <session/ZfsSession.h>
#include <toolkit/QuotaTk.h>
#include "GenericDebugMsgEx.h"



#define GENDBGMSG_OP_LISTOPENFILES          "listopenfiles"
#define GENDBGMSG_OP_VERSION                "version"
#define GENDBGMSG_OP_MSGQUEUESTATS          "msgqueuestats"
#define GENDBGMSG_OP_RESYNCQUEUELEN         "resyncqueuelen"
#define GENDBGMSG_OP_CHUNKLOCKSTORESIZE     "chunklockstoresize"
#define GENDBGMSG_OP_CHUNKLOCKSTORECONTENTS "chunklockstore"
#define GENDBGMSG_OP_SETREJECTIONRATE       "setrejectionrate"


bool GenericDebugMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GenericDebugMsg incoming");

   LOG_DEBUG_CONTEXT(log, 5, std::string("Command string: ") + getCommandStr() );

   std::string cmdRespStr = processCommand();

   ctx.sendResponse(GenericDebugRespMsg(cmdRespStr.c_str() ) );

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), StorageOpCounter_GENERICDEBUG,
      getMsgHeaderUserID() );

   return true;
}

/**
 * @return command response string
 */
std::string GenericDebugMsgEx::processCommand()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   std::string responseStr;
   std::string operation;

   // load command string into a stream to allow us to use getline
   std::istringstream commandStream(getCommandStr() );

   // get operation type from command string
   std::getline(commandStream, operation, ' ');

   if(operation == GENDBGMSG_OP_LISTOPENFILES)
      responseStr = processOpListOpenFiles(commandStream);
   else
   if(operation == GENDBGMSG_OP_VERSION)
      responseStr = processOpVersion(commandStream);
   else
   if(operation == GENDBGMSG_OP_MSGQUEUESTATS)
      responseStr = processOpMsgQueueStats(commandStream);
   else
   if(operation == GENDBGMSG_OP_VARLOGMESSAGES)
      responseStr = MsgHelperGenericDebug::processOpVarLogMessages(commandStream);
   else
   if(operation == GENDBGMSG_OP_VARLOGKERNLOG)
      responseStr = MsgHelperGenericDebug::processOpVarLogKernLog(commandStream);
   else
   if(operation == GENDBGMSG_OP_FHGFSLOG)
      responseStr = MsgHelperGenericDebug::processOpFhgfsLog(commandStream);
   else
   if(operation == GENDBGMSG_OP_LOADAVG)
      responseStr = MsgHelperGenericDebug::processOpLoadAvg(commandStream);
   else
   if(operation == GENDBGMSG_OP_DROPCACHES)
      responseStr = MsgHelperGenericDebug::processOpDropCaches(commandStream);
   else
   if(operation == GENDBGMSG_OP_GETCFG)
      responseStr = MsgHelperGenericDebug::processOpCfgFile(commandStream, cfg->getCfgFile() );
   else
   if(operation == GENDBGMSG_OP_GETLOGLEVEL)
      responseStr = MsgHelperGenericDebug::processOpGetLogLevel(commandStream);
   else
   if(operation == GENDBGMSG_OP_SETLOGLEVEL)
      responseStr = MsgHelperGenericDebug::processOpSetLogLevel(commandStream);
   else
   if(operation == GENDBGMSG_OP_NETOUT)
      responseStr = MsgHelperGenericDebug::processOpNetOut(commandStream,
         app->getMgmtNodes(), app->getMetaNodes(), app->getStorageNodes() );
   else
   if(operation == GENDBGMSG_OP_QUOTAEXCEEDED)
      responseStr = processOpQuotaExceeded(commandStream);
   else
   if(operation == GENDBGMSG_OP_USEDQUOTA)
      responseStr = processOpUsedQuota(commandStream);
   else
   if(operation == GENDBGMSG_OP_RESYNCQUEUELEN)
      responseStr = processOpResyncQueueLen(commandStream);
   else
   if(operation == GENDBGMSG_OP_CHUNKLOCKSTORESIZE)
      responseStr = processOpChunkLockStoreSize(commandStream);
   else
   if(operation == GENDBGMSG_OP_CHUNKLOCKSTORECONTENTS)
      responseStr = processOpChunkLockStoreContents(commandStream);
   else
   if(operation == GENDBGMSG_OP_LISTSTORAGESTATES)
      responseStr = MsgHelperGenericDebug::processOpListTargetStates(commandStream,
         app->getTargetStateStore() );
   else
   if(operation == GENDBGMSG_OP_SETREJECTIONRATE)
      responseStr = processOpSetRejectionRate(commandStream);
   else
      responseStr = "Unknown/invalid operation";

   return responseStr;
}

std::string GenericDebugMsgEx::processOpListOpenFiles(std::istringstream& commandStream)
{
   // protocol: no arguments

   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();

   std::ostringstream responseStream;
   NumNodeIDList sessionIDs;
   size_t numFilesTotal = 0;
   size_t numCheckedSessions = 0; // may defer from number of initially queried sessions

   size_t numSessions = sessions->getAllSessionIDs(&sessionIDs);

   responseStream << "Found " << numSessions << " sessions." << std::endl;

   responseStream << std::endl;

   // walk over all sessions
   for(NumNodeIDListCIter iter = sessionIDs.begin(); iter != sessionIDs.end(); iter++)
   {
      // note: sessionID might have become removed since we queried it, e.g. because client is gone

      auto session = sessions->referenceSession(*iter);
      if(!session)
         continue;

      numCheckedSessions++;

      SessionLocalFileStore* sessionFiles = session->getLocalFiles();

      size_t numFiles = sessionFiles->getSize();

      if(!numFiles)
         continue; // only print sessions with open files

      numFilesTotal += numFiles;

      responseStream << *iter << ": " << numFiles << std::endl;
   }

   responseStream << std::endl;

   responseStream << "Final results: " << numFilesTotal << " open files in " <<
      numCheckedSessions << " checked sessions";

   return responseStream.str();
}

std::string GenericDebugMsgEx::processOpVersion(std::istringstream& commandStream)
{
   return BEEGFS_VERSION;
}

std::string GenericDebugMsgEx::processOpMsgQueueStats(std::istringstream& commandStream)
{
   // protocol: no arguments

   App* app = Program::getApp();
   MultiWorkQueueMap* workQueueMap = app->getWorkQueueMap();

   std::ostringstream responseStream;
   std::string indirectQueueStats;
   std::string directQueueStats;
   std::string busyStats;

   for(MultiWorkQueueMapCIter iter = workQueueMap->begin(); iter != workQueueMap->end(); iter++)
   {
      MultiWorkQueue* workQ = iter->second;

      workQ->getStatsAsStr(indirectQueueStats, directQueueStats, busyStats);

      responseStream << "* [queue id " << iter->first << "] "
         "general queue stats: " << std::endl <<
         indirectQueueStats << std::endl;

      responseStream << "* [queue id " << iter->first << "] "
         "direct queue stats: " << std::endl <<
         directQueueStats << std::endl;

      responseStream << "* [queue id " << iter->first << "] "
         "busy worker stats: " << std::endl <<
         busyStats << std::endl;
   }

   return responseStream.str();
}

std::string GenericDebugMsgEx::processOpQuotaExceeded(std::istringstream& commandStream)
{
   App* app = Program::getApp();

   std::string targetIdStr;
   std::getline(commandStream, targetIdStr, ' ');
   uint16_t targetId = StringTk::strToUInt(targetIdStr);

   if(!app->getConfig()->getQuotaEnableEnforcement() )
      return "No quota exceeded IDs on this storage daemon because quota enforcement is"
         "disabled.";

   ExceededQuotaStorePtr exQuotaStore = app->getExceededQuotaStores()->get(targetId);
   // exQuotaStore may be null;needs to be checked in MsgHelperGenericDebug::processOpQuotaExceeded
   return MsgHelperGenericDebug::processOpQuotaExceeded(commandStream, exQuotaStore.get());
}

std::string GenericDebugMsgEx::processOpUsedQuota(std::istringstream& commandStream)
{
   App *app = Program::getApp();

   std::ostringstream responseStream;

   ZfsSession session;
   QuotaDataType quotaDataType = QuotaDataType_NONE;
   std::string quotaDataTypeStr;
   bool forEachTarget = false;
   unsigned rangeStart = 0;
   unsigned rangeEnd = 0;

   // get parameter from command string
   std::string inputString;
   while(!commandStream.eof() )
   {
      std::getline(commandStream, inputString, ' ');

      if(inputString == "uid")
      {
         quotaDataType = QuotaDataType_USER;
         quotaDataTypeStr = "user";
      }
      else
      if(inputString == "gid")
      {
         quotaDataType = QuotaDataType_GROUP;
         quotaDataTypeStr = "group";
      }
      else
      if(inputString == "forEachTarget")
         forEachTarget = true;
      else
      if(inputString == "range")
      {
         std::string rangeValue;
         std::getline(commandStream, rangeValue, ' ');
         rangeStart = StringTk::strToUInt(rangeValue);
         std::getline(commandStream, rangeValue, ' ');
         rangeEnd = StringTk::strToUInt(rangeValue);
      }
   }

   // verify given parameters
   if(quotaDataType == QuotaDataType_NONE)
      return "Invalid or missing quota data type argument.";
   if(rangeStart == 0 && rangeEnd == 0)
      return "Invalid or missing range argument.";


   if(forEachTarget)
   {
      const auto& targets = app->getStorageTargets()->getTargets();

      responseStream << "Quota data of " << targets.size() << " targets." << std::endl;

      for (const auto& mapping : targets)
      {
         const auto& target = *mapping.second;

         QuotaDataList outQuotaDataList;

         QuotaBlockDeviceMap quotaBlockDevices = {
            {mapping.first, target.getQuotaBlockDevice()}
         };

         QuotaTk::requestQuotaForRange(&quotaBlockDevices, rangeStart, rangeEnd, quotaDataType,
            &outQuotaDataList, &session);

         responseStream << outQuotaDataList.size() << " used quota for " << quotaDataTypeStr
            << " IDs on target: " << mapping.first << std::endl;

         QuotaData::quotaDataListToString(outQuotaDataList, &responseStream);
      }
   }
   else
   {
      auto& targets = app->getStorageTargets()->getTargets();

      QuotaBlockDeviceMap quotaBlockDevices;

      std::transform(
            targets.begin(), targets.end(),
            std::inserter(quotaBlockDevices, quotaBlockDevices.end()),
            [] (const auto& target) {
               return std::make_pair(target.first, target.second->getQuotaBlockDevice());
            });

      QuotaDataList outQuotaDataList;

      QuotaTk::requestQuotaForRange(&quotaBlockDevices, rangeStart, rangeEnd, quotaDataType,
         &outQuotaDataList, &session);

      QuotaData::quotaDataListToString(outQuotaDataList, &responseStream);
   }

   return responseStream.str();
}


std::string GenericDebugMsgEx::processOpResyncQueueLen(std::istringstream& commandStream)
{
   // protocol: targetID files/dirs as argument (e.g. "resyncqueuelen 1234 files")

   // get parameter from command string
   std::string targetIDStr;
   uint16_t targetID;
   std::string typeStr;
   std::getline(commandStream, targetIDStr, ' ');
   std::getline(commandStream, typeStr, ' ');
   targetID = StringTk::strToUInt(targetIDStr);

   if (targetID == 0)
      return "Invalid or missing targetID";

   BuddyResyncJob* resyncJob = Program::getApp()->getBuddyResyncer()->getResyncJob(targetID);

   if (!resyncJob)
      return "0";

   if (typeStr == "files")
   {
      size_t count = resyncJob->syncCandidates.getNumFiles();
      return StringTk::uintToStr(count);
   }
   else
   if (typeStr == "dirs")
   {
      size_t count = resyncJob->syncCandidates.getNumDirs();
      return StringTk::uintToStr(count);
   }
   else
      return "Invalid or missing queue type";
}

std::string GenericDebugMsgEx::processOpChunkLockStoreSize(std::istringstream& commandStream)
{
   // protocol: targetID as argument (e.g. "chunklockstoresize 1234")

   // get parameter from command string
   std::string targetIDStr;
   uint16_t targetID;
   std::getline(commandStream, targetIDStr, ' ');
   targetID = StringTk::strToUInt(targetIDStr);

   if (targetID == 0)
      return "Invalid or missing targetID";

   size_t lockStoreSize = Program::getApp()->getChunkLockStore()->getSize(targetID);

   return StringTk::uintToStr(lockStoreSize);
}

std::string GenericDebugMsgEx::processOpChunkLockStoreContents(std::istringstream& commandStream)
{
   // protocol: targetID and size limit (optional) as argument (e.g. "chunklockstoresize 1234 50")
   std::stringstream outStream;

   // get parameter from command string
   std::string targetIDStr;
   uint16_t targetID;
   std::string maxEntriesStr;
   unsigned maxEntries;
   std::getline(commandStream, targetIDStr, ' ');
   targetID = StringTk::strToUInt(targetIDStr);
   std::getline(commandStream, maxEntriesStr, ' ');
   maxEntries = StringTk::strToUInt(maxEntriesStr);

   if (targetID == 0)
      return "Invalid or missing targetID";

   StringSet lockStoreContents = Program::getApp()->getChunkLockStore()->getLockStoreCopy(targetID);
   unsigned lockStoreSize = lockStoreContents.size();
   StringSetIter lockStoreIter = lockStoreContents.begin();

   if ( (maxEntries == 0) || (maxEntries > lockStoreSize) )
      maxEntries = lockStoreSize;

   for (unsigned i = 0; i < maxEntries; i++)
   {
      outStream << *lockStoreIter << std::endl;
      lockStoreIter++;
   }

   return outStream.str();
}

std::string GenericDebugMsgEx::processOpSetRejectionRate(std::istringstream& commandStream)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   std::string rejectionRateStr;
   std::ostringstream responseStream;

   std::getline(commandStream, rejectionRateStr, ' ');
   unsigned rejectionRate = StringTk::strToUInt(rejectionRateStr);

   cfg->setConnectionRejectionRate(rejectionRate);

   responseStream << "Setting connection reject rate to " << rejectionRate << std::endl;
   return responseStream.str();
}

