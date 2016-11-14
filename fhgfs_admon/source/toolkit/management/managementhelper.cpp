#include <common/net/message/admon/GetNodeInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoMsg.h>
#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/SetDirPatternMsg.h>
#include <common/net/message/storage/attribs/SetDirPatternRespMsg.h>
#include <common/net/message/storage/attribs/StatMsg.h>
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringMsg.h>
#include <common/net/message/storage/mirroring/SetMetadataMirroringRespMsg.h>
#include <common/nodes/Node.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/Raid10Pattern.h>
#include <common/storage/striping/BuddyMirrorPattern.h>
#include <common/toolkit/MetadataTk.h>
#include <program/Program.h>
#include <toolkit/bashtk.h>
#include "managementhelper.h"



namespace managementhelper
{
   std::string formatDateSec(uint64_t sec)
   {
      time_t ctime = (time_t) sec;
      return asctime(localtime(&(ctime)));
   }

   std::string formatFileMode(int mode)
   {
      const char *rwx = "rwxrwxrwx";
      int bits[] =
         { S_IRUSR, S_IWUSR, S_IXUSR, /*Permissions User*/
         S_IRGRP, S_IWGRP, S_IXGRP, /*Group*/
         S_IROTH, S_IWOTH, S_IXOTH /*All*/
         };

      char res[10];
      res[0] = '\0';
      for (short i = 0; i < 9; i++)
      {
         res[i] = (mode & bits[i]) ? rwx[i] : '-';
      }
      res[9] = '\0';
      return std::string(res);
   }

   /**
    * @return false on error (e.g. communication error or path does not exist)
    */
   bool getEntryInfo(std::string pathStr, std::string *outChunkSize,
      std::string *outNumTargets, UInt16Vector *outPrimaryTargetNumIDs, unsigned* outPatternID,
      UInt16Vector *outSecondaryTargetNumIDs, UInt16Vector *outStorageBMGs,
      NumNodeID* outPrimaryMetaNodeNumID, NumNodeID* outSecondaryMetaNodeNumID,
      uint16_t* outMetaBMG)
   {
      bool retVal = false;

      App* app = Program::getApp();
      Logger* log = app->getLogger();
      auto metaNodes = app->getMetaNodes();
      auto metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();
      auto storageBuddyGroupMapper = app->getStorageBuddyGroupMapper();


      // find owner node
      Path path("/" + pathStr);

      NodeHandle ownerNode;
      EntryInfo entryInfo;

      FhgfsOpsErr findRes = MetadataTk::referenceOwner(&path, false, metaNodes, ownerNode,
         &entryInfo, metaBuddyGroupMapper);

      if (findRes != FhgfsOpsErr_SUCCESS)
      {
         log->logErr("getEntryInfo", "Unable to find metadata node for path: " + pathStr +
            ". Error: " + FhgfsOpsErrTk::toErrString(findRes) );
      }
      else
      {
         bool commRes;
         char* respBuf = NULL;
         NetMessage* respMsg = NULL;
         GetEntryInfoRespMsg* respMsgCast;

         GetEntryInfoMsg getInfoMsg(&entryInfo);

         commRes = MessagingTk::requestResponse(*ownerNode, &getInfoMsg,
            NETMSGTYPE_GetEntryInfoResp, &respBuf, &respMsg);

         if (commRes)
         {
            respMsgCast = (GetEntryInfoRespMsg*) respMsg;

            StripePattern& pattern = respMsgCast->getPattern();

            *outChunkSize = StringTk::uintToStr(pattern.getChunkSize() );
            *outNumTargets = StringTk::uintToStr(pattern.getDefaultNumTargets() );
            *outPatternID = pattern.getPatternType();

            if(*outPatternID == StripePatternType_Raid10)
            {
               *outPrimaryTargetNumIDs = *pattern.getStripeTargetIDs();
               *outSecondaryTargetNumIDs = *pattern.getMirrorTargetIDs();
            }
            else if(*outPatternID == StripePatternType_BuddyMirror)
            {
               *outStorageBMGs = *pattern.getStripeTargetIDs();

               for(UInt16VectorIter iter = outStorageBMGs->begin(); iter != outStorageBMGs->end();
                  iter++)
               {
                  outPrimaryTargetNumIDs->push_back(
                     storageBuddyGroupMapper->getPrimaryTargetID(*iter) );
                  outSecondaryTargetNumIDs->push_back(
                     storageBuddyGroupMapper->getSecondaryTargetID(*iter) );
               }
            }
            else
            {
               *outPrimaryTargetNumIDs = *pattern.getStripeTargetIDs();
            }

            if (entryInfo.getIsBuddyMirrored() )
            {
               *outMetaBMG = ownerNode->getNumID().val();
               *outPrimaryMetaNodeNumID = NumNodeID(metaBuddyGroupMapper->getPrimaryTargetID(
                  ownerNode->getNumID().val() ) );
               *outSecondaryMetaNodeNumID = NumNodeID(metaBuddyGroupMapper->getSecondaryTargetID(
                  ownerNode->getNumID().val() ) );
            }
            else
            {
               *outMetaBMG = 0;
               *outPrimaryMetaNodeNumID = ownerNode->getNumID();
               *outSecondaryMetaNodeNumID = NumNodeID(0);
            }

            retVal = true;
         }
         else
         {
            log->logErr("getEntryInfo", "Error during communication with owner node. Node: " +
               ownerNode->getNodeIDWithTypeStr() + ". Path: " + pathStr + ". Error: " +
               FhgfsOpsErrTk::toErrString(findRes) );
         }

         SAFE_DELETE(respMsg);
         SAFE_FREE(respBuf);
      }

      return retVal;
   }

   /**
    * @param patternID pattern type (STRIPEPATTERN_...)
    * @return false on error
    */
   bool setPattern(std::string pathStr, uint chunkSize, uint defaultNumNodes,
      unsigned patternID, bool doMetaMirroring)
   {
      bool retVal = false;

      App* app = Program::getApp();
      Logger* log = app->getLogger();
      auto metaNodes = app->getMetaNodes();
      auto metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

      // find the owner node of the path
      NodeHandle ownerNode;
      Path path("/" + pathStr);
      EntryInfo entryInfo;

      log->log(Log_SPAM, "setPattern", std::string("Set pattern for path: ") + pathStr );

      FhgfsOpsErr findRes = MetadataTk::referenceOwner(&path, false, metaNodes, ownerNode,
         &entryInfo, metaBuddyGroupMapper);

      if(findRes != FhgfsOpsErr_SUCCESS)
      {
         log->logErr("setPattern", "Unable to find metadata node for path: " + pathStr + "Error: " +
            std::string(FhgfsOpsErrTk::toErrString(findRes) ) );
         return retVal;
      }

      char* respBuf = NULL;
      char* respBufMirror = NULL;
      NetMessage* respMsg = NULL;

      // generate a new StripePattern with an empty list of stripe nodes (will be
      // ignored on the server side)

      UInt16Vector stripeNodeIDs;
      UInt16Vector mirrorTargetIDs;
      StripePattern* pattern = NULL;

      if (patternID == StripePatternType_Raid0)
         pattern = new Raid0Pattern(chunkSize, stripeNodeIDs, defaultNumNodes);
      else
      if (patternID == StripePatternType_Raid10)
         pattern = new Raid10Pattern(chunkSize, stripeNodeIDs, mirrorTargetIDs, defaultNumNodes);
      else
      if (patternID == StripePatternType_BuddyMirror)
         pattern = new BuddyMirrorPattern(chunkSize, stripeNodeIDs, defaultNumNodes);
      else
      {
         log->logErr("setPattern", "Set pattern: Invalid/unknown pattern type number: " +
            StringTk::uintToStr(patternID) );
      }

      if(pattern)
      {
         // send SetDirPatternMsg

         SetDirPatternMsg setPatternMsg(&entryInfo, pattern);

         bool commRes = MessagingTk::requestResponse(*ownerNode, &setPatternMsg,
            NETMSGTYPE_SetDirPatternResp, &respBuf, &respMsg);

         if(!commRes)
         {
            log->logErr("setPattern", "Communication failed with node: " +
               ownerNode->getNodeIDWithTypeStr() );

            goto cleanup;
         }

         SetDirPatternRespMsg *setDirPatternRespMsg = (SetDirPatternRespMsg *)respMsg;
         FhgfsOpsErr result = setDirPatternRespMsg->getResult();

         if (result != FhgfsOpsErr_SUCCESS)
         {
            log->logErr("setPattern", "Modification of stripe pattern failed on server. "
               "Path: " + pathStr + "; "
               "Server: " + ownerNode->getNodeIDWithTypeStr() + "; "
               "Error: " + FhgfsOpsErrTk::toErrString(result) );

            goto cleanup;
         }

         // successfully modified on owner
         retVal = true;
      }

      if(doMetaMirroring)
      {
         NetMessage* respMsgMirror = NULL;
         SetMetadataMirroringRespMsg* respMsgCast;

         FhgfsOpsErr setRemoteRes;

         SetMetadataMirroringMsg setMsg;

         // request/response
         bool commRes = MessagingTk::requestResponse(*ownerNode, &setMsg,
            NETMSGTYPE_SetMetadataMirroringResp, &respBufMirror, &respMsgMirror);
         if(!commRes)
         {
            log->logErr("setPattern", "Communication with server failed: " +
               ownerNode->getNodeIDWithTypeStr() );
            goto cleanup;
         }

         respMsgCast = (SetMetadataMirroringRespMsg*)respMsgMirror;

         setRemoteRes = respMsgCast->getResult();
         if(setRemoteRes != FhgfsOpsErr_SUCCESS)
         {
            log->logErr("setPattern", "Operation failed on server: " +
               ownerNode->getNodeIDWithTypeStr() + "; " + "Error: " +
               FhgfsOpsErrTk::toErrString(setRemoteRes) );
            goto cleanup;
         }

         retVal = true;
      }

   cleanup:
      SAFE_DELETE(respMsg);
      SAFE_FREE(respBuf);
      SAFE_DELETE(pattern);
      SAFE_FREE(respBufMirror);

      return retVal;
   }

   /*
    * list a directory from a given offset (list incrementally)
    *
    * @param pathStr The path to be listed
    * @param offset The starting offset; will be modified to represent ending offset
    * @param outEntries reference to a FileEntryList used as result list
    * @param tmpTotalSize total size of the read entries in byte
    * @param count Amount of entries to read
    */
   int listDirFromOffset(std::string pathStr, int64_t *offset,
      FileEntryList *outEntries, uint64_t *tmpTotalSize, short count)
   {
      App* app = Program::getApp();
      Logger* log = app->getLogger();
      auto metaNodes = app->getMetaNodes();
      auto metaBuddyGroupMapper = app->getMetaBuddyGroupMapper();

      log->log(Log_SPAM, "listDirFromOffset", std::string("Listing content of path: ") + pathStr);
      // find owner node
      *tmpTotalSize = 0;
      Path path("/" + pathStr);

      NodeHandle ownerNode;
      EntryInfo dirEntryInfo;

      FhgfsOpsErr findRes = MetadataTk::referenceOwner(&path, false, metaNodes, ownerNode,
         &dirEntryInfo, metaBuddyGroupMapper);
      if (findRes != FhgfsOpsErr_SUCCESS)
      {
         log->logErr("listDirFromOffset", "Unable to find metadata node for path: " + pathStr +
            ". (Error: " + FhgfsOpsErrTk::toErrString(findRes) + ")");
         return -1;
      }
      else
      {
         // get the contents

         bool commRes;
         char* respListBuf = NULL;
         NetMessage* respListDirMsg = NULL;
         ListDirFromOffsetRespMsg* listDirRespMsg;
         ListDirFromOffsetMsg listDirMsg(&dirEntryInfo, *offset, count, true);

         commRes = MessagingTk::requestResponse(*ownerNode, &listDirMsg,
            NETMSGTYPE_ListDirFromOffsetResp, &respListBuf, &respListDirMsg);
         StringList* fileNames;

         if (commRes)
         {
            listDirRespMsg = (ListDirFromOffsetRespMsg*) respListDirMsg;
            fileNames = &listDirRespMsg->getNames();
            *offset = listDirRespMsg->getNewServerOffset();
         }
         else
         {
            SAFE_FREE(respListBuf);
            SAFE_DELETE(respListDirMsg);
            return -1;
         }

         // get attributes
         NetMessage* respAttrMsg = NULL;
         StringListIter nameIter;

         /* check for equal length of fileNames and fileTypes is
          * in ListDirFromOffsetRespMsg::deserializePayload() */
         for (nameIter = fileNames->begin(); nameIter != fileNames->end();
            nameIter++)
         {
            std::string nameStr = pathStr + "/" + *nameIter;
            Path tmpPath(nameStr);
            EntryInfo entryInfo;
            NodeHandle entryOwnerNode;

            if (MetadataTk::referenceOwner(&tmpPath, false, metaNodes,
               entryOwnerNode, &entryInfo, metaBuddyGroupMapper) == FhgfsOpsErr_SUCCESS)
            {
               StatRespMsg* statRespMsg;

               StatMsg statMsg(&entryInfo);
               char* respAttrBuf = NULL;
               commRes = MessagingTk::requestResponse(*entryOwnerNode, &statMsg,
                  NETMSGTYPE_StatResp, &respAttrBuf, &respAttrMsg);
               if (commRes)
               {
                  statRespMsg = (StatRespMsg*) respAttrMsg;

                  StatData* statData = statRespMsg->getStatData();

                  FileEntry entry;
                  entry.name = *nameIter;
                  entry.statData.set(statData);

                  outEntries->push_back(entry);
                  *tmpTotalSize += entry.statData.getFileSize();
               }
               else
               {
                  SAFE_DELETE(respAttrMsg);
                  SAFE_FREE(respAttrBuf);
                  SAFE_FREE(respListBuf);
                  SAFE_DELETE(respListDirMsg);

                  return -1;
               }
               SAFE_DELETE(respAttrMsg);
               SAFE_FREE(respAttrBuf);
            }
            else
            {
               log->logErr("listDirFromOffset", "Metadata for entry " + *nameIter +
                  " could not be read.");
            }
         }
         SAFE_FREE(respListBuf);
         SAFE_DELETE(respListDirMsg);
      }
      return 0;
   }

   int file2HTML(std::string filename, std::string *outStr)
   {
      std::ifstream file(filename.c_str());

      if (!(file.is_open()))
         return -1;

      while (!file.eof())
      {
         std::string line;
         getline(file, line);
         *outStr += line + "<br>";
      }

      file.close();
      return 0;
   }

   /**
    * Reads a complete file into outStr.
    *
    * @return 0 on success, !=0 on error
    */
   int getFileContents(std::string filename, std::string *outStr)
   {
      std::ifstream file(filename.c_str());
      if(!(file.is_open() ) )
         return -1;

      while(!file.eof() && file.good() )
      {
         std::string line;
         getline(file, line);

         // filter escape characters
         /* note: ("\33" is octal.) this escape character is e.g. used to start coloring in bash. there
            are actually more characters for color codes following, but parsing those would be too
            complex here, so we only remove the escape char, for the admon GUI presentation. */

         if (line.empty() && file.eof())
         {
            break;
         }

         size_t pos = line.find('\33');
         while(pos != std::string::npos)
         {
            line.erase(pos, 1);
            pos = line.find('\33');
         }
         // note: "\r" is added here in addition to "\n" for windows GUI compatibility.
         *outStr += line + "\r\n";
      }

      file.close();

      return 0;
   }

   int startService(std::string service, StringList *failedNodes)
   {
      ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

      Mutex mutex;
      SafeMutexLock mutexLock(&mutex);

      Job *job = jobRunner->addJob(BASH + " " + SCRIPT_START_PARALLEL + " " + service, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);

      mutexLock.unlock();

      std::ifstream outFile((job->outputFile).c_str());
      if (!(outFile.is_open()))
      {
         jobRunner->delJob(job->id);
         return -2;
      }

      while (!outFile.eof())
      {
         std::string line;
         getline(outFile, line);
         if (StringTk::trim(line) != "")
         {
            failedNodes->push_back(line);
         }
      }

      outFile.close();

      jobRunner->delJob(job->id);

      if (!failedNodes->empty())
      {
         return -1;
      }

      return 0;
   }

   int startService(std::string service, std::string host)
   {
      ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

      Mutex mutex;
      SafeMutexLock mutexLock(&mutex);

      Job *job = jobRunner->addJob(BASH + " " + SCRIPT_START_PARALLEL + " " + service + " " + host,
         &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);

      mutexLock.unlock();

      int retVal = job->returnCode;
      jobRunner->delJob(job->id);

      return retVal;
   }

   int stopService(std::string service, StringList *failedNodes)
   {
      ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

      Mutex mutex;
      SafeMutexLock mutexLock(&mutex);

      Job *job = jobRunner->addJob(BASH + " " + SCRIPT_STOP_PARALLEL + " " + service, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);

      mutexLock.unlock();

      std::ifstream outFile((job->outputFile).c_str());
      if (!(outFile.is_open()))
      {
         jobRunner->delJob(job->id);
         return -2;
      }

      while (!outFile.eof())
      {
         std::string line;
         getline(outFile, line);
         if (StringTk::trim(line) != "")
         {
            failedNodes->push_back(line);
         }
      }

      outFile.close();

      jobRunner->delJob(job->id);

      if (!failedNodes->empty())
      {
         return -1;
      }

      return 0;
   }


   int stopService(std::string service, std::string host)
   {
      ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

      Mutex mutex;
      SafeMutexLock mutexLock(&mutex);

      Job *job = jobRunner->addJob(BASH + " " + SCRIPT_STOP + " " + service + " " + host, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);

      mutexLock.unlock();

      int retVal = job->returnCode;
      jobRunner->delJob(job->id);

      return retVal;
   }

   int checkStatus(std::string service, std::string host, std::string *status)
   {
      ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

      Mutex mutex;
      SafeMutexLock mutexLock(&mutex);

      Job *job = jobRunner->addJob(BASH + " " + SCRIPT_CHECK_STATUS + " " + service
         + " " + host, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);

      mutexLock.unlock();

      if (job->returnCode != 0)
      {
         jobRunner->delJob(job->id);
         return -1;
      }

      std::ifstream outFile((job->outputFile).c_str());
      if (!(outFile.is_open()))
      {
         jobRunner->delJob(job->id);
         return -2;
      }

      while (!outFile.eof())
      {
         std::string line;
         getline(outFile, line);
         *status += line;
      }

      outFile.close();

      jobRunner->delJob(job->id);

      return 0;
   }

   int checkStatus(std::string service, StringList *runningHosts,
      StringList *stoppedHosts)
   {
      ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

      Mutex mutex;
      SafeMutexLock mutexLock(&mutex);

      Job *job = jobRunner->addJob(BASH + " " + SCRIPT_CHECK_STATUS_PARALLEL + " " + service, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);

      mutexLock.unlock();

      std::ifstream outFile((job->outputFile).c_str());
      if (!(outFile.is_open()))
      {
         jobRunner->delJob(job->id);
         return -2;
      }

      while (!outFile.eof())
      {
         std::string line;
         getline(outFile, line);
         StringList parsedLine;
         StringTk::explode(line, ',', &parsedLine);

         if (parsedLine.size() < 2)
            continue;

         if (StringTk::strToBool(parsedLine.back()))
         {
            runningHosts->push_back(parsedLine.front());
         }
         else
         {
            stoppedHosts->push_back(parsedLine.front());
         }
      }

      outFile.close();

      jobRunner->delJob(job->id);

      return 0;
   }

   void restartAdmon()
   {
      Program::setRestart(true);
      Program::getApp()->stopComponents();
   }

   int getLogFile(NodeType nodeType, uint16_t nodeNumID, uint lines,
      std::string *outStr, std::string nodeID)
   {
      Logger* log = Program::getApp()->getLogger();

      *outStr = "";

      // get the path to the log file

      std::string logFilePath;
      std::string hostname;

      if (nodeType == NODETYPE_Mgmt)
      {
         NodeStoreMgmtEx *nodeStore = Program::getApp()->getMgmtNodes();
         auto node = nodeStore->referenceNode(NumNodeID(nodeNumID) );
         if (node == NULL)
         {
            log->log(Log_ERR, __FUNCTION__, "Could not reference management server. nodeNumID: " +
               StringTk::uintToStr(nodeNumID) );
             return -1;
         }

         logFilePath = ( (MgmtNodeEx*)node.get())->getGeneralInformation().logFilePath;
         hostname = ( (MgmtNodeEx*)node.get())->getID();
      }
      else
      if (nodeType == NODETYPE_Meta)
      {
         NodeStoreMetaEx *nodeStore = Program::getApp()->getMetaNodes();
         auto node =  nodeStore->referenceNode(NumNodeID(nodeNumID) );
         if (node == NULL)
         {
            log->log(Log_ERR, __FUNCTION__, "Could not reference metadata server. nodeNumID: " +
               StringTk::uintToStr(nodeNumID) );
             return -1;
         }

         logFilePath = ( (MetaNodeEx*)node.get())->getGeneralInformation().logFilePath;
         hostname = ( (MetaNodeEx*)node.get())->getID();
      }
      else
      if (nodeType == NODETYPE_Storage)
      {
         NodeStoreStorageEx *nodeStore = Program::getApp()->getStorageNodes();
         auto node =  nodeStore->referenceNode(NumNodeID(nodeNumID) );
         if (node == NULL)
         {
            log->log(Log_ERR, __FUNCTION__, "Could not reference storage server. nodeNumID: " +
               StringTk::uintToStr(nodeNumID) );
             return -1;
         }

         logFilePath = ( (StorageNodeEx*)node.get())->getGeneralInformation().logFilePath;
         hostname = ( (StorageNodeEx*)node.get())->getID();
      }
      else
      if (nodeType == NODETYPE_Client)
      {
         logFilePath = CLIENT_LOGFILEPATH;

         StringList idParts;
         StringTk::explode(nodeID, '-', &idParts);

         if(idParts.size() > 2)
         {
            StringListIter startIter = idParts.begin();
            std::advance(startIter, 2);
            StringList hostnameList(startIter, idParts.end() );
            hostname = StringTk::implode('-', hostnameList, false);
         }
      }

      if(hostname.empty() )
      {
         log->log(Log_ERR, __FUNCTION__, "Destination hostname is empty. nodeID: " + nodeID +
            "; nodeNumID: " + StringTk::uintToStr(nodeNumID) +
            "; nodeType: " + Node::nodeTypeToStr(nodeType) );
         return -1;
      }

      ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

      Mutex mutex;
      SafeMutexLock mutexLock(&mutex);

      std::string cmdLine = BASH + " " + SCRIPT_GET_REMOTE_FILE + " " + hostname + " " +
         logFilePath + " " + StringTk::uintToStr(lines);
      Job *job = jobRunner->addJob(cmdLine, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);

      mutexLock.unlock();

      if(job->returnCode == 0)
      {
         managementhelper::getFileContents(job->outputFile, outStr);
      }
      else
      {
         jobRunner->delJob(job->id);
         return -1;
      }

      jobRunner->delJob(job->id);
      return 0;
   }
} /* namespace managementhelper */
