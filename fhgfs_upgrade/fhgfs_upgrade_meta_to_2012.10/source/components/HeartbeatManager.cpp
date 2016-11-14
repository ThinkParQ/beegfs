#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/RefreshCapacityPoolsMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/SessionTk.h>
#include <common/toolkit/SocketTk.h>
#include <net/msghelpers/MsgHelperClose.h>
#include <program/Program.h>
#include "HeartbeatManager.h"


HeartbeatManager::HeartbeatManager(DatagramListener* dgramLis) throw(ComponentInitException) :
   PThread("HBeatMgr")
{
   log.setContext("HBeatMgr");

   this->cfg = Program::getApp()->getConfig();
   this->dgramLis = dgramLis;

   this->mgmtNodes = Program::getApp()->getMgmtNodes();
   this->metaNodes = Program::getApp()->getMetaNodes();
   this->storageNodes = Program::getApp()->getStorageNodes();
   this->clientNodes = Program::getApp()->getClientNodes();

   this->nodeRegistered = false;
   this->mgmtInitDone = false;
}

HeartbeatManager::~HeartbeatManager()
{
}


void HeartbeatManager::run()
{
   try
   {
      registerSignalHandler();

      mgmtInit();

      requestLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }

   signalMgmtInitDone();
}

void HeartbeatManager::requestLoop()
{
   const int sleepTimeMS = 5000;
   const unsigned registrationIntervalMS = 60000;
   const unsigned downloadNodesIntervalMS = 300000;

   Time lastRegistrationT;
   Time lastDownloadNodesT;


   while(!waitForSelfTerminateOrder(sleepTimeMS) )
   {
      // registration
      if(!nodeRegistered)
      {
         if((lastRegistrationT.elapsedMS() > registrationIntervalMS) )
         {
            if(registerNode() )
            {
               forceMgmtdPoolsRefresh();
               downloadAndSyncNodes();
               downloadAndSyncTargetMappings();
               Program::getApp()->getInternodeSyncer()->setForcePoolsUpdate();
            }

            lastRegistrationT.setToNow();
         }

         continue;
      }

      // the following things only happen after successful node registration...

      // download & sync nodes
      if(lastDownloadNodesT.elapsedMS() > downloadNodesIntervalMS)
      {
         downloadAndSyncNodes();
         downloadAndSyncTargetMappings();

         lastDownloadNodesT.setToNow();
      }

   } // end of while loop
}


void HeartbeatManager::mgmtInit()
{
   log.log(Log_WARNING, "Waiting for management node...");

   if(!NodesTk::waitForMgmtHeartbeat(this, dgramLis, mgmtNodes,
      cfg->getSysMgmtdHost(), cfg->getConnMgmtdPortUDP() ) )
      return;

   log.log(Log_NOTICE, "Management node found. Downloading node groups...");

   downloadAndSyncNodes();
   downloadAndSyncTargetMappings();

   log.log(Log_NOTICE, "Node registration...");

   if(registerNode() )
   { // download nodes again now that we will receive notifications about add/remove (avoids race)
      forceMgmtdPoolsRefresh();
      downloadAndSyncNodes();
      downloadAndSyncTargetMappings();
      Program::getApp()->getInternodeSyncer()->setForcePoolsUpdate();
   }

   signalMgmtInitDone();

   log.log(Log_NOTICE, "Init complete.");
}


void HeartbeatManager::downloadAndSyncNodes()
{
   Node* mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return;

   App* app = Program::getApp();
   Node* localNode = app->getLocalNode();

   // metadata nodes

   NodeList metaNodesList;
   UInt16List addedMetaNodes;
   UInt16List removedMetaNodes;
   uint16_t rootNodeID;
   bool rootIsBuddyMirrored;

   if(NodesTk::downloadNodes(mgmtNode, NODETYPE_Meta, &metaNodesList, &rootNodeID) )
   {
      metaNodes->syncNodes(&metaNodesList, &addedMetaNodes, &removedMetaNodes, true,
         localNode);
      if(metaNodes->setRootNodeNumID(rootNodeID, false, rootIsBuddyMirrored) )
      {
         log.log(Log_CRITICAL, std::string("Root node ID (from sync results): ") +
            StringTk::uintToStr(rootNodeID) );
         app->getRootDir()->setOwnerNodeID(rootNodeID);
      }

      printSyncResults(NODETYPE_Meta, &addedMetaNodes, &removedMetaNodes);
   }

   // storage nodes

   NodeList storageNodesList;
   UInt16List addedStorageNodes;
   UInt16List removedStorageNodes;

   if(NodesTk::downloadNodes(mgmtNode, NODETYPE_Storage, &storageNodesList) )
   {
      storageNodes->syncNodes(&storageNodesList, &addedStorageNodes, &removedStorageNodes, true,
         localNode);
      printSyncResults(NODETYPE_Storage, &addedStorageNodes, &removedStorageNodes);
   }

   // clients

   NodeList clientNodesList;
   StringList addedClientNodes;
   StringList removedClientNodes;

   if(NodesTk::downloadNodes(mgmtNode, NODETYPE_Client, &clientNodesList) )
   {
      /* copy the nodesList, because syncNodes() and syncClients() both will remove all elements
         from their lists */
      NodeList clientNodesListCopy;
      NodesTk::copyListNodes(&clientNodesList, &clientNodesListCopy);

      clientNodes->syncNodes(&clientNodesList, &addedClientNodes, &removedClientNodes, true,
         localNode);

      syncClients(&clientNodesListCopy, true); // sync client sessions
   }


   mgmtNodes->releaseNode(&mgmtNode);
}

void HeartbeatManager::printSyncResults(NodeType nodeType, UInt16List* addedNodes,
   UInt16List* removedNodes)
{
   if(addedNodes->size() )
      log.log(Log_WARNING, std::string("Nodes added (sync results): ") +
         StringTk::uintToStr(addedNodes->size() ) +
         " (Type: " + Node::nodeTypeToStr(nodeType) + ")");

   if(removedNodes->size() )
      log.log(Log_WARNING, std::string("Nodes removed (sync results): ") +
         StringTk::uintToStr(removedNodes->size() ) +
         " (Type: " + Node::nodeTypeToStr(nodeType) + ")");
}

void HeartbeatManager::downloadAndSyncTargetMappings()
{
   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();

   Node* mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return;

   UInt16List targetIDs;
   UInt16List nodeIDs;

   bool downloadRes = NodesTk::downloadTargetMappings(mgmtNode, &targetIDs, &nodeIDs);
   if(downloadRes)
      targetMapper->syncTargetsFromLists(targetIDs, nodeIDs);

   mgmtNodes->releaseNode(&mgmtNode);
}

/**
 * @return true if an ack was received for the heartbeat, false otherwise
 */
bool HeartbeatManager::registerNode()
{
   static bool registrationFailureLogged = false; // to avoid log spamming

   Node* mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   Node* localNode = Program::getApp()->getLocalNode();
   std::string localNodeID = localNode->getID();
   uint16_t localNodeNumID = localNode->getNumID();
   uint16_t rootNodeID = metaNodes->getRootNodeNumID();
   NicAddressList nicList(localNode->getNicList() );

   HeartbeatMsg msg(localNodeID.c_str(), localNodeNumID, NODETYPE_Meta, rootNodeID, &nicList);
   msg.setPorts(cfg->getConnMetaPortUDP(), cfg->getConnMetaPortTCP() );
   msg.setFhgfsVersion(FHGFS_VERSION_CODE);

   this->nodeRegistered = dgramLis->sendToNodeUDPwithAck(mgmtNode, &msg);

   mgmtNodes->releaseNode(&mgmtNode);

   if(nodeRegistered)
      log.log(Log_WARNING, "Node registration successful.");
   else
   if(!registrationFailureLogged)
   {
      log.log(Log_CRITICAL, "Node registration not successful. Management node offline? "
         "Will keep on trying...");
      registrationFailureLogged = true;
   }

   return nodeRegistered;
}

/**
 * Tell mgmtd to update its capacity pools (after we have registered ourselves).
 *
 * @return true if an ack was received for our message, false otherwise
 */
bool HeartbeatManager::forceMgmtdPoolsRefresh()
{
   Node* mgmtNode = mgmtNodes->referenceFirstNode();
   if(!mgmtNode)
      return false;

   RefreshCapacityPoolsMsg msg;

   bool ackReceived = dgramLis->sendToNodeUDPwithAck(mgmtNode, &msg);

   mgmtNodes->releaseNode(&mgmtNode);

   return ackReceived;
}

/**
 * Synchronize local client sessions with registered mgmtd clients to release orphaned sessions.
 *
 * @param clientsList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param allowRemoteComm usually true; setting this to false is only useful when called during
 * app shutdown to avoid communication; if false, unlocking of user locks, closing of storage server
 * files and disposal of unlinked files won't be performed
 */
void HeartbeatManager::syncClients(NodeList* clientsList, bool allowRemoteComm)
{
   App* app = Program::getApp();
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   SessionStore* sessions = app->getSessions();

   SessionList removedSessions;
   StringList unremovableSessions;

   sessions->syncSessions(clientsList, &removedSessions, &unremovableSessions);

   // print client sessions results (upfront)
   if(removedSessions.size() || unremovableSessions.size() )
   {
      std::ostringstream logMsgStream;
      logMsgStream << "Removing " << removedSessions.size() << " client sessions. " <<
         "(" << unremovableSessions.size() << " are unremovable)";
      log.log(Log_WARNING, logMsgStream.str() );
   }


   // walk over all removed sessions (to cleanup the contained files)

   SessionListIter sessionIter = removedSessions.begin();
   for( ; sessionIter != removedSessions.end(); sessionIter++)
   { // walk over all client sessions: cleanup each session
      Session* session = *sessionIter;
      std::string sessionID = session->getSessionID();
      SessionFileStore* sessionFiles = session->getFiles();

      SessionFileList removedSessionFiles;
      UIntList referencedSessionFiles;

      sessionFiles->removeAllSessions(&removedSessionFiles, &referencedSessionFiles);

      /* note: referencedSessionFiles should always be empty, because otherwise the reference holder
         would also hold a reference to the client session (and we woudn't be here if the client
         session had any references) */


      // print session files results (upfront)

      if(removedSessionFiles.size() || referencedSessionFiles.size() )
      {
         std::ostringstream logMsgStream;
         logMsgStream << sessionID << ": Removing " << removedSessionFiles.size() <<
            " file sessions. " << "(" << referencedSessionFiles.size() << " are unremovable)";
         log.log(Log_NOTICE, logMsgStream.str() );
      }


      // walk over all files in the current session (to clean them up)

      SessionFileListIter fileIter = removedSessionFiles.begin();

      for( ; fileIter != removedSessionFiles.end(); fileIter++)
      { // walk over all files: unlock user locks, close meta, close local, dispose unlinked
         SessionFile* sessionFile = *fileIter;
         unsigned ownerFD = sessionFile->getSessionID();
         unsigned accessFlags = sessionFile->getAccessFlags();
         unsigned numHardlinks;
         unsigned numInodeRefs;

         FileInode* inode = sessionFile->getInode();
         std::string fileID = inode->getEntryID();

         std::string fileHandleID = SessionTk::generateFileHandleID(ownerFD, fileID);

         // save nodeIDs for later
         StripePattern* pattern = inode->getStripePattern();
         int maxUsedNodesIndex = pattern->getStripeTargetIDs()->size() - 1;

         if(allowRemoteComm)
         { // unlock all user locks
            inode->flockEntryCancelByClientID(sessionID);
            inode->flockRangeCancelByClientID(sessionID);
         }

         if(allowRemoteComm)
            MsgHelperClose::closeChunkFile(sessionID.c_str(), fileHandleID.c_str(),
               maxUsedNodesIndex, inode);

         EntryInfo* entryInfo = sessionFile->getEntryInfo();
         log.log(Log_NOTICE, std::string("closing file ") + "ParentID: " +
            entryInfo->getParentEntryID() + " FileName: " + entryInfo->getFileName() );

         metaStore->closeFile(entryInfo, inode, accessFlags, &numHardlinks, &numInodeRefs);

         delete(sessionFile);

         if(allowRemoteComm && !numHardlinks && !numInodeRefs)
            MsgHelperClose::unlinkDisposableFile(fileID);
      } // end of files loop


      delete(session);

   } // end of client sessions loop
}

void HeartbeatManager::signalMgmtInitDone()
{
   SafeMutexLock mutexLock(&mgmtInitDoneMutex);

   mgmtInitDone = true;
   mgmtInitDoneCond.broadcast();

   mutexLock.unlock();
}

void HeartbeatManager::waitForMmgtInit()
{
   SafeMutexLock mutexLock(&mgmtInitDoneMutex);

   while(!mgmtInitDone)
      mgmtInitDoneCond.wait(&mgmtInitDoneMutex);

   mutexLock.unlock();
}

