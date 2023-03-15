#include <common/net/message/nodes/GenericDebugRespMsg.h>
#include <common/net/msghelpers/MsgHelperGenericDebug.h>
#include <common/storage/quota/Quota.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include <session/SessionStore.h>
#include "GenericDebugMsgEx.h"


#define GENDBGMSG_OP_LISTFILEAPPENDLOCKS  "listfileappendlocks"
#define GENDBGMSG_OP_LISTFILEENTRYLOCKS   "listfileentrylocks"
#define GENDBGMSG_OP_LISTFILERANGELOCKS   "listfilerangelocks"
#define GENDBGMSG_OP_LISTOPENFILES        "listopenfiles"
#define GENDBGMSG_OP_REFERENCESTATISTICS  "refstats"
#define GENDBGMSG_OP_CACHESTATISTICS      "cachestats"
#define GENDBGMSG_OP_VERSION              "version"
#define GENDBGMSG_OP_MSGQUEUESTATS        "msgqueuestats"
#define GENDBGMSG_OP_LISTPOOLS            "listpools"
#define GENDBGMSG_OP_DUMPDENTRY           "dumpdentry"
#define GENDBGMSG_OP_DUMPINODE            "dumpinode"
#define GENDBGMSG_OP_DUMPINLINEDINODE     "dumpinlinedinode"

#ifdef BEEGFS_DEBUG
   #define GENDBGMSG_OP_WRITEDIRDENTRY       "writedirdentry"
   #define GENDBGMSG_OP_WRITEDIRINODE        "writedirinode"
   #define GENDBGMSG_OP_WRITEFILEINODE       "writefileinode"
#endif // BEEGFS_DEBUG


bool GenericDebugMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GenericDebugMsg incoming");

   LOG_DEBUG_CONTEXT(log, 5, std::string("Command string: ") + getCommandStr() );

   std::string cmdRespStr = processCommand();

   ctx.sendResponse(GenericDebugRespMsg(cmdRespStr.c_str() ) );

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

   if(operation == GENDBGMSG_OP_LISTFILEAPPENDLOCKS)
      responseStr = processOpListFileAppendLocks(commandStream);
   else
   if(operation == GENDBGMSG_OP_LISTFILEENTRYLOCKS)
      responseStr = processOpListFileEntryLocks(commandStream);
   else
   if(operation == GENDBGMSG_OP_LISTFILERANGELOCKS)
      responseStr = processOpListFileRangeLocks(commandStream);
   else
   if(operation == GENDBGMSG_OP_LISTOPENFILES)
      responseStr = processOpListOpenFiles(commandStream);
   else
   if(operation == GENDBGMSG_OP_REFERENCESTATISTICS)
      responseStr = processOpReferenceStatistics(commandStream);
   else
   if(operation == GENDBGMSG_OP_CACHESTATISTICS)
      responseStr = processOpCacheStatistics(commandStream);
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
   if(operation == GENDBGMSG_OP_LISTMETASTATES)
      responseStr = MsgHelperGenericDebug::processOpListTargetStates(commandStream,
         app->getMetaStateStore() );
   else
   if(operation == GENDBGMSG_OP_LISTSTORAGESTATES)
      responseStr = MsgHelperGenericDebug::processOpListTargetStates(commandStream,
         app->getTargetStateStore() );
   else
   if(operation == GENDBGMSG_OP_LISTPOOLS)
      responseStr = processOpListPools(commandStream);
   else
   if(operation == GENDBGMSG_OP_DUMPDENTRY)
      responseStr = processOpDumpDentry(commandStream);
   else
   if(operation == GENDBGMSG_OP_DUMPINODE)
      responseStr = processOpDumpInode(commandStream);
   else
   if(operation == GENDBGMSG_OP_DUMPINLINEDINODE)
      responseStr = processOpDumpInlinedInode(commandStream);
   else
   if(operation == GENDBGMSG_OP_QUOTAEXCEEDED)
      responseStr = processOpQuotaExceeded(commandStream);
   else if(operation == GENDBGMSG_OP_LISTSTORAGEPOOLS)
      responseStr = MsgHelperGenericDebug::processOpListStoragePools(commandStream,
         app->getStoragePoolStore());
#ifdef BEEGFS_DEBUG
   else
   if(operation == GENDBGMSG_OP_WRITEDIRDENTRY)
      responseStr = processOpWriteDirDentry(commandStream);
   else
   if(operation == GENDBGMSG_OP_WRITEDIRINODE)
      responseStr = processOpWriteDirInode(commandStream);
   else
   if(operation == GENDBGMSG_OP_WRITEFILEINODE)
      responseStr = processOpWriteInlinedFileInode(commandStream);
#endif // BEEGFS_DEBUG
   else
      responseStr = "Unknown/invalid operation";

   return responseStr;
}

/**
 * Retrieve append lock stats for a certain file.
 */
std::string GenericDebugMsgEx::processOpListFileAppendLocks(std::istringstream& commandStream)
{
   // procotol: entryID as only argument

   std::string parentEntryID;
   std::string entryID;
   std::string responseStr;
   std::string isBuddyMirroredStr;
   bool isBuddyMirrored;

   // get entryID from command string
   std::getline(commandStream, parentEntryID, ' ');
   std::getline(commandStream, entryID, ' ');
   std::getline(commandStream, isBuddyMirroredStr, ' ');

   if (parentEntryID.empty() )
      return "Invalid or missing parentEntryID";

   if(entryID.empty() )
      return "Invalid or missing entryID";

   if(isBuddyMirroredStr.empty())
      isBuddyMirrored = false;
   else
      isBuddyMirrored = StringTk::strToBool(isBuddyMirroredStr);

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   MetaFileHandle inode = metaStore->referenceLoadedFile(parentEntryID, isBuddyMirrored, entryID);
   if(!inode)
      return "FileID not exists: " + entryID;

   responseStr = inode->flockAppendGetAllAsStr();

   metaStore->releaseFile(parentEntryID, inode);

   return responseStr;
}

std::string GenericDebugMsgEx::processOpListFileEntryLocks(std::istringstream& commandStream)
{
   // procotol: entryID as only argument

   std::string parentEntryID;
   std::string entryID;
   std::string responseStr;
   std::string isBuddyMirroredStr;
   bool isBuddyMirrored;

   // get entryID from command string
   std::getline(commandStream, parentEntryID, ' ');
   std::getline(commandStream, entryID, ' ');
   std::getline(commandStream, isBuddyMirroredStr, ' ');

   if (parentEntryID.empty() )
      return "Invalid or missing parentEntryID";

   if(entryID.empty() )
      return "Invalid or missing entryID";

   if(isBuddyMirroredStr.empty())
      isBuddyMirrored = false;
   else
      isBuddyMirrored = StringTk::strToBool(isBuddyMirroredStr);

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   MetaFileHandle inode = metaStore->referenceLoadedFile(parentEntryID, isBuddyMirrored, entryID);
   if(!inode)
      return "FileID not exists: " + entryID;

   responseStr = inode->flockEntryGetAllAsStr();

   metaStore->releaseFile(parentEntryID, inode);

   return responseStr;
}

std::string GenericDebugMsgEx::processOpListFileRangeLocks(std::istringstream& commandStream)
{
   // procotol: entryID as only argument

   std::string parentEntryID;
   std::string entryID;
   std::string isBuddyMirroredStr;
   bool isBuddyMirrored;

   // get parentEntryID from command string
   std::getline(commandStream, parentEntryID, ' ');

   if(parentEntryID.empty() )
      return "Invalid or missing parentEntryID";

   // get entryID from command string
   std::getline(commandStream, entryID, ' ');

   if(entryID.empty() )
      return "Invalid or missing entryID";

   // get isBuddyMirrored from command string
   std::getline(commandStream, isBuddyMirroredStr, ' ');

   if(isBuddyMirroredStr.empty())
      isBuddyMirrored = false;
   else
      isBuddyMirrored = StringTk::strToBool(isBuddyMirroredStr);

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   MetaFileHandle file = metaStore->referenceLoadedFile(parentEntryID, isBuddyMirrored, entryID);
   if(!file)
      return "FileID not found: " + entryID;

   std::string responseStr = file->flockRangeGetAllAsStr();

   metaStore->releaseFile(parentEntryID, file);

   return responseStr;
}

std::string GenericDebugMsgEx::processOpListOpenFiles(std::istringstream& commandStream)
{
   // protocol: no arguments

   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();
   SessionStore* mirroredSessions = app->getMirroredSessions();

   std::ostringstream responseStream;
   size_t numFilesTotal = 0;
   size_t numCheckedSessions = 0; // may defer from number of initially queried sessions

   NumNodeIDList sessionIDs = sessions->getAllSessionIDs();
   NumNodeIDList mirroredSessionIDs = mirroredSessions->getAllSessionIDs();

   responseStream << "Found " << sessionIDs.size() << " non-mirrored sessions and "
      << mirroredSessionIDs.size() << " mirrored sessions." << std::endl;

   responseStream << std::endl;

   responseStream << "Non-mirrored sessions:" << std::endl;
   // walk over all sessions
   for(NumNodeIDListCIter iter = sessionIDs.begin(); iter != sessionIDs.end(); iter++)
   {
      Session* session = sessions->referenceSession(*iter, false);
      // note: sessionID might have been removed since we queried it, e.g. because client is gone
      if(!session)
         continue;

      numCheckedSessions++;

      SessionFileStore* sessionFiles = session->getFiles();
      size_t numFiles = sessionFiles->getSize();
      sessions->releaseSession(session);

      if(!numFiles)
         continue; // only print sessions with open files

      numFilesTotal += numFiles;

      responseStream << *iter << ": " << numFiles << std::endl;
   }

   responseStream << "Mirrored sessions:" << std::endl;
   // ...and the mirrored sessions
   for(NumNodeIDListCIter iter = mirroredSessionIDs.begin(); iter != mirroredSessionIDs.end();
       ++iter)
   {
      Session* session = mirroredSessions->referenceSession(*iter, false);
      if (!session)
         continue;

      numCheckedSessions++;

      SessionFileStore* sessionFiles = session->getFiles();
      size_t numFiles = sessionFiles->getSize();
      mirroredSessions->releaseSession(session);

      if (!numFiles)
         continue;

      numFilesTotal += numFiles;
      responseStream << *iter << ": " << numFiles << std::endl;
   }

   responseStream << std::endl;

   responseStream << "Final results: " << numFilesTotal << " open files in " <<
      numCheckedSessions << " checked sessions";

   return responseStream.str();
}

std::string GenericDebugMsgEx::processOpReferenceStatistics(std::istringstream& commandStream)
{
   // protocol: no arguments

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   std::ostringstream responseStream;
   size_t numDirs;
   size_t numFiles;

   metaStore->getReferenceStats(&numDirs, &numFiles);

   responseStream << "Dirs: " << numDirs << std::endl;
   responseStream << "Files: " << numFiles;

   return responseStream.str();
}

std::string GenericDebugMsgEx::processOpCacheStatistics(std::istringstream& commandStream)
{
   // protocol: no arguments

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   std::ostringstream responseStream;
   size_t numDirs;

   metaStore->getCacheStats(&numDirs);

   responseStream << "Dirs: " << numDirs;

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
   MultiWorkQueue* workQ = app->getWorkQueue();

   std::ostringstream responseStream;
   std::string indirectQueueStats;
   std::string directQueueStats;
   std::string busyStats;

   workQ->getStatsAsStr(indirectQueueStats, directQueueStats, busyStats);

   responseStream << "general queue stats: " << std::endl <<
      indirectQueueStats << std::endl;

   responseStream << "direct queue stats: " << std::endl <<
      directQueueStats << std::endl;

   responseStream << "busy worker stats: " << std::endl <<
      busyStats << std::endl;

   return responseStream.str();
}

/**
 * List internal state of meta and storage capacity pools.
 */
std::string GenericDebugMsgEx::processOpListPools(std::istringstream& commandStream)
{
   // protocol: no arguments

   const App* app = Program::getApp();
   const NodeCapacityPools* metaPools = app->getMetaCapacityPools();

   std::ostringstream responseStream;

   responseStream << "META POOLS STATE: " << std::endl << metaPools->getStateAsStr() << std::endl;

   const StoragePoolPtrVec storagePools = app->getStoragePoolStore()->getPoolsAsVec();

   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      const TargetCapacityPools* capPool = (*iter)->getTargetCapacityPools();

      responseStream << "STORAGE CAPACITY POOLS STATE (STORAGE POOL ID: " << (*iter)->getId()
                     << "): " << std::endl << capPool->getStateAsStr() << std::endl;
   }

   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      const NodeCapacityPools* capPool = (*iter)->getBuddyCapacityPools();

      responseStream << "STORAGE BUDDY CAPACITY POOLS STATE (STORAGE POOL ID: "
                     << (*iter)->getId() << "): " << std::endl << capPool->getStateAsStr()
                     << std::endl;
   }

   return responseStream.str();
}

std::string GenericDebugMsgEx::processOpDumpDentry(std::istringstream& commandStream)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   std::ostringstream responseStream;

   StringList parameterList;
   StringTk::explode(commandStream.str(), ' ', &parameterList);

   if ( parameterList.size() < 3 || parameterList.size() > 4 )
      return "Invalid or missing parameters; Parameter format: parentDirID entryName "
         "[isBuddyMirrored]";

   StringListIter iter = parameterList.begin();
   iter++;
   std::string parentDirID = *iter;
   iter++;
   std::string entryName = *iter;
   iter++;
   bool isBuddyMirrored = false;
   if (iter != parameterList.end())
   {
      isBuddyMirrored = StringTk::strToBool(*iter);
   }

   DirInode* parentDirInode = metaStore->referenceDir(parentDirID, isBuddyMirrored, false);

   if (!parentDirInode)
   return "Unable to reference parent directory.";

   DirEntry dentry(entryName);
   bool getDentryRes = parentDirInode->getDentry(entryName, dentry);

   metaStore->releaseDir(parentDirID);

   if (!getDentryRes)
   return "Unable to get dentry from parent directory.";

   responseStream << "entryType: " << dentry.getEntryType() << std::endl;
   responseStream << "ID: " << dentry.getID() << std::endl;
   responseStream << "ownerNodeID: " << dentry.getOwnerNodeID() << std::endl;
   responseStream << "featureFlags: " << dentry.getDentryFeatureFlags() << std::endl;

   return responseStream.str();
}

#ifdef BEEGFS_DEBUG
std::string GenericDebugMsgEx::processOpWriteDirDentry(std::istringstream& commandStream)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   std::string dentriesPath = Program::getApp()->getDentriesPath()->str();

   std::ostringstream responseStream;

   StringList parameterList;
   StringTk::explode(commandStream.str(), ' ', &parameterList);

   if ( parameterList.size() < 4 || parameterList.size() > 5 )
      return "Invalid or missing parameters; Parameter format: parentDirID entryName ownerNodeID "
         "[isBuddyMirrored]";

   StringListIter iter = parameterList.begin();
   iter++;
   std::string parentDirID = *iter;
   iter++;
   std::string entryName = *iter;
   iter++;
   NumNodeID ownerNodeID(StringTk::strToUInt(*iter) );

   iter++;
   bool isBuddyMirrored = false;
   if (iter!=parameterList.end())
   {
      isBuddyMirrored = StringTk::strToBool(*iter);
   }

   DirInode* parentDirInode = metaStore->referenceDir(parentDirID, isBuddyMirrored, true);

   if (!parentDirInode)
      return "Unable to reference parent directory.";

   DirEntry dentry(entryName);
   bool getDentryRes = parentDirInode->getDentry(entryName, dentry);

   metaStore->releaseDir(parentDirID);

   if (!getDentryRes)
      return "Unable to get dentry from parent directory.";

   bool setOwnerRes = dentry.setOwnerNodeID(
      MetaStorageTk::getMetaDirEntryPath(dentriesPath, parentDirID), ownerNodeID);

   if (!setOwnerRes)
      return "Unable to set new owner node ID in dentry.";

   return "OK";
}
#endif // BEEGFS_DEBUG

std::string GenericDebugMsgEx::processOpDumpInode(std::istringstream& commandStream)
{
   // commandStream: ID of inode

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   std::ostringstream responseStream;

   std::string inodeID;
   std::string isBuddyMirroredStr;
   bool isBuddyMirrored;

   // get inodeID from command string
   std::getline(commandStream, inodeID, ' ');

   if(inodeID.empty() )
      return "Invalid or missing inode ID";

   // get isBuddyMirrored from command string
   std::getline(commandStream, isBuddyMirroredStr, ' ');
   if(isBuddyMirroredStr.empty())
      isBuddyMirrored = false;
   else
      isBuddyMirrored = StringTk::strToBool(isBuddyMirroredStr);

   MetaFileHandle fileInode;
   DirInode* dirInode = NULL;

   metaStore->referenceInode(inodeID, isBuddyMirrored, fileInode, dirInode);

   if (fileInode)
   {
      StatData statData;
      if (fileInode->getStatData(statData) != FhgfsOpsErr_SUCCESS)
      { // stat data retrieval failed
         metaStore->releaseFile("", fileInode);
         return "Could not get stat data for requested file inode";
      }

      DirEntryType dirEntryType = MetadataTk::posixFileTypeToDirEntryType(fileInode->getMode() );

      std::string parentDirID = "cannotBeUsed";
      uint16_t parentNodeID   = 0;

      responseStream << "entryType: " << dirEntryType << std::endl;
      responseStream << "parentEntryID: " <<  parentDirID << std::endl;
      responseStream << "parentNodeID: " << StringTk::uintToStr(parentNodeID) << std::endl;
      responseStream << "mode: " << StringTk::intToStr(statData.getMode()) << std::endl;
      responseStream << "uid: " << StringTk::uintToStr(statData.getUserID()) << std::endl;
      responseStream << "gid: " << StringTk::uintToStr(statData.getGroupID()) << std::endl;
      responseStream << "filesize: " << StringTk::int64ToStr(statData.getFileSize()) << std::endl;
      responseStream << "ctime: " << StringTk::int64ToStr(statData.getCreationTimeSecs()) << std::endl;
      responseStream << "atime: " << StringTk::int64ToStr(statData.getLastAccessTimeSecs()) << std::endl;
      responseStream << "mtime: " << StringTk::int64ToStr(statData.getModificationTimeSecs()) << std::endl;
      responseStream << "hardlinks: " << StringTk::intToStr(statData.getNumHardlinks()) << std::endl;
      responseStream << "stripeTargets: "
         << StringTk::uint16VecToStr(fileInode->getStripePattern()->getStripeTargetIDs())
         << std::endl;
      responseStream << "chunkSize: "
         << StringTk::uintToStr(fileInode->getStripePattern()->getChunkSize()) << std::endl;
      responseStream << "featureFlags: " << fileInode->getFeatureFlags() << std::endl;

      metaStore->releaseFile("", fileInode);
   }
   else
   if (dirInode)
   {
      StatData statData;
      if (dirInode->getStatData(statData) != FhgfsOpsErr_SUCCESS)
      { // stat data retrieval failed
         metaStore->releaseDir(inodeID);
         return "Could not get stat data for requested dir inode";
      }

      DirEntryType dirEntryType = MetadataTk::posixFileTypeToDirEntryType(dirInode->getMode() );

      std::string parentDirID;
      NumNodeID parentNodeID;
      dirInode->getParentInfo(&parentDirID, &parentNodeID);

         responseStream << "entryType: " << dirEntryType << std::endl;
         responseStream << "parentEntryID: " << parentDirID << std::endl;
         responseStream << "parentNodeID: " << parentNodeID.str() << std::endl;
         responseStream << "ownerNodeID: " << dirInode->getOwnerNodeID().str() << std::endl;
         responseStream << "mode: " << StringTk::intToStr(statData.getMode()) << std::endl;
         responseStream << "uid: " << StringTk::uintToStr(statData.getUserID()) << std::endl;
         responseStream << "gid: " << StringTk::uintToStr(statData.getGroupID()) << std::endl;
         responseStream << "size: " << StringTk::int64ToStr(statData.getFileSize()) << std::endl;
         responseStream << "numLinks: " << StringTk::int64ToStr(statData.getNumHardlinks())
            << std::endl;
         responseStream << "ctime: " << StringTk::int64ToStr(statData.getCreationTimeSecs())
            << std::endl;
         responseStream << "atime: " << StringTk::int64ToStr(statData.getLastAccessTimeSecs())
            << std::endl;
         responseStream << "mtime: " << StringTk::int64ToStr(statData.getModificationTimeSecs())
            << std::endl;
         responseStream << "featureFlags: " << dirInode->getFeatureFlags() << std::endl;

      metaStore->releaseDir(inodeID);
   }
   else
   {
      return "Could not read requested inode";
   }

   return responseStream.str();
}

std::string GenericDebugMsgEx::processOpDumpInlinedInode(std::istringstream& commandStream)
{
   // commandStream: parentID, name

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   NumNodeID localNodeID = Program::getApp()->getLocalNode().getNumID();
   std::ostringstream responseStream;

   StringList parameterList;
   StringTk::explode(commandStream.str(), ' ', &parameterList);

   if ( parameterList.size() < 3 || parameterList.size() > 4 )
      return "Invalid or missing parameters; Parameter format: parentDirID entryName "
         "[isBuddyMirrored]";

   StringListIter iter = parameterList.begin();
   iter++;
   std::string parentEntryID = *iter;
   iter++;
   std::string entryName = *iter;
   iter++;
   bool isBuddyMirrored = false;
   if (iter != parameterList.end())
   {
      isBuddyMirrored = StringTk::strToBool(*iter);
   }

   EntryInfo entryInfo(localNodeID, parentEntryID, "unknown", entryName, DirEntryType_REGULARFILE,
      0);

   DirInode* parentInode = metaStore->referenceDir(parentEntryID, isBuddyMirrored, false);

   if ( !parentInode )
      return "Could not open parent directory";

   DirEntry dirEntry(entryName);
   bool getDentryRes = parentInode->getDentry(entryName, dirEntry);

   if ( !getDentryRes )
   {
      metaStore->releaseDir(parentEntryID);
      return "Could not open dir entry";
   }

   FileInodeStoreData* inodeData = dirEntry.getInodeStoreData();

   if ( !inodeData )
   {
      metaStore->releaseDir(parentEntryID);
      return "Could not get inlined inode data";
   }

   StatData* statData = inodeData->getInodeStatData();
   if ( !statData )
   {
      metaStore->releaseDir(parentEntryID);
      return "Could not get stat data for requested file inode";
   }

   responseStream << "entryID: " << inodeData->getEntryID() << std::endl;
   responseStream << "mode: " << StringTk::intToStr(statData->getMode()) << std::endl;
   responseStream << "uid: " << StringTk::uintToStr(statData->getUserID()) << std::endl;
   responseStream << "gid: " << StringTk::uintToStr(statData->getGroupID()) << std::endl;
   responseStream << "filesize: " << StringTk::int64ToStr(statData->getFileSize()) << std::endl;
   responseStream << "ctime: " << StringTk::int64ToStr(statData->getCreationTimeSecs()) << std::endl;
   responseStream << "atime: " << StringTk::int64ToStr(statData->getLastAccessTimeSecs())
      << std::endl;
   responseStream << "mtime: " << StringTk::int64ToStr(statData->getModificationTimeSecs())
      << std::endl;
   responseStream << "hardlinks: " << StringTk::intToStr(statData->getNumHardlinks()) << std::endl;
   responseStream << "stripeTargets: "
      << StringTk::uint16VecToStr(inodeData->getPattern()->getStripeTargetIDs()) << std::endl;
   responseStream << "chunkSize: "
      << StringTk::uintToStr(inodeData->getPattern()->getChunkSize()) << std::endl;
   responseStream << "featureFlags: " << inodeData->getInodeFeatureFlags() << std::endl;

   metaStore->releaseDir(parentEntryID);

   return responseStream.str();
}

std::string GenericDebugMsgEx::processOpQuotaExceeded(std::istringstream& commandStream)
{
   App* app = Program::getApp();

   std::string targetIdStr;
   std::getline(commandStream, targetIdStr, ' ');
   uint16_t targetId = StringTk::strToUInt(targetIdStr);

   std::string returnString;

   if(!app->getConfig()->getQuotaEnableEnforcement() )
      return "No quota exceeded IDs on this storage daemon because quota enforcement is"
         "disabled.";

   ExceededQuotaStorePtr exQuotaStore = app->getExceededQuotaStores()->get(targetId);
   // exQuotaStore may be null;needs to be checked in MsgHelperGenericDebug::processOpQuotaExceeded
   return MsgHelperGenericDebug::processOpQuotaExceeded(commandStream, exQuotaStore.get());
}

#ifdef BEEGFS_DEBUG
std::string GenericDebugMsgEx::processOpWriteDirInode(std::istringstream& commandStream)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   // get parameters from command string
   StringVector paramVec;
   StringTk::explode(commandStream.str(), ' ', &paramVec);

   std::string entryID;
   std::string parentDirID;
   NumNodeID parentNodeID;
   NumNodeID ownerNodeID;
   int mode;
   uint uid;
   uint gid;
   int64_t size;
   unsigned numLinks;
   bool isBuddyMirrored;

   try
   {
      unsigned i = 1;
      entryID = paramVec.at(i++);
      parentDirID = paramVec.at(i++);
      parentNodeID = NumNodeID(StringTk::strToUInt(paramVec.at(i++)));
      ownerNodeID = NumNodeID(StringTk::strToUInt(paramVec.at(i++)));
      mode = StringTk::strToInt(paramVec.at(i++));
      uid = StringTk::strToUInt(paramVec.at(i++));
      gid = StringTk::strToUInt(paramVec.at(i++));
      size = StringTk::strToInt64(paramVec.at(i++));
      numLinks = StringTk::strToUInt(paramVec.at(i++));
      if (i<paramVec.size())
         isBuddyMirrored = StringTk::strToBool(paramVec.at(i));
      else
         isBuddyMirrored = false;
   }
   catch (std::out_of_range& e)
   {
      std::string paramFormatStr =
         "entryID parentDirID parentNodeID ownerNodeID mode uid gid size numLinks "
         "[isBuddyMirrored]";
      return "Invalid or missing parameters; Parameter format: " + paramFormatStr;
   }

   DirInode* dirInode = metaStore->referenceDir(entryID, isBuddyMirrored, true);

   if ( !dirInode )
      return "Could not find directory with ID: " + entryID;

   StatData statData;
   if ( dirInode->getStatData(statData) != FhgfsOpsErr_SUCCESS )
   {
      metaStore->releaseDir(entryID);
      return "Could not get stat data for requested dir inode";
   }

   dirInode->setParentInfoInitial(parentDirID, parentNodeID);
   dirInode->setOwnerNodeID(ownerNodeID);

   statData.setMode(mode);
   statData.setUserID(uid);
   statData.setGroupID(gid);
   statData.setFileSize(size);
   statData.setNumHardLinks(numLinks);

   dirInode->setStatData(statData);

   metaStore->releaseDir(entryID);

   return "OK";
}


std::string GenericDebugMsgEx::processOpWriteInlinedFileInode(std::istringstream& commandStream)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   std::string retStr = "OK";

   // get parameters from command string
   StringVector paramVec;
   StringTk::explode(commandStream.str(), ' ', &paramVec);

   std::string parentDirID;
   std::string name;
   std::string entryID;
   int mode;
   uint uid;
   uint gid;
   int64_t filesize;
   unsigned numLinks;
   UInt16Vector stripeTargets;
 //  UInt16Vector* origStripeTargets;

   try
   {
      unsigned i = 1;
      parentDirID = paramVec.at(i++);
      name = paramVec.at(i++);
      entryID = paramVec.at(i++);
      mode = StringTk::strToInt(paramVec.at(i++));
      uid = StringTk::strToUInt(paramVec.at(i++));
      gid = StringTk::strToUInt(paramVec.at(i++));
      filesize = StringTk::strToInt64(paramVec.at(i++));
      numLinks = StringTk::strToUInt(paramVec.at(i++));
      StringTk::strToUint16Vec(paramVec.at(i++), &stripeTargets);
   }
   catch (std::out_of_range& e)
   {
      std::string paramFormatStr =
         "parentDirID entryName entryID mode uid gid filesize numLinks stripeTargets";
      return "Invalid or missing parameters; Parameter format: " + paramFormatStr;
   }

   EntryInfo entryInfo(Program::getApp()->getLocalNodeNumID(), parentDirID, entryID, name,
      DirEntryType_REGULARFILE, 0);

   MetaFileHandle fileInode = metaStore->referenceFile(&entryInfo);

   if (!fileInode)
      return "Could not reference inode";

   StatData statData;
   fileInode->getStatData(statData);

   statData.setMode(mode);
   statData.setUserID(uid);
   statData.setGroupID(gid);
   statData.setFileSize(filesize);
   statData.setNumHardLinks(numLinks);

   fileInode->setStatData(statData);

   StripePattern* pattern = fileInode->getStripePattern();
   UInt16Vector* origTargets = pattern->getStripeTargetIDsModifyable();
   *origTargets = stripeTargets;

   fileInode->updateInodeOnDisk(&entryInfo, pattern);

   metaStore->releaseFile(parentDirID, fileInode);

   return retStr;
}
#endif // BEEGFS_DEBUG
