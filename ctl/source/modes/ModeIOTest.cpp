#include <app/App.h>
#include <common/components/worker/ReadLocalFileV2Work.h>
#include <common/components/worker/WriteLocalFileWork.h>
#include <common/net/message/session/opening/OpenFileMsg.h>
#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <common/net/message/session/opening/CloseFileMsg.h>
#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/toolkit/TimeFine.h>
#include <common/toolkit/UnitTk.h>
#include <program/Program.h>
#include "ModeIOTest.h"

#include <boost/lexical_cast.hpp>


#define MODEIOTEST_ARG_UNMOUNTEDPATH            "--unmounted"
#define MODEIOTEST_ARG_WRITE                    "--write"
#define MODEIOTEST_ARG_READ                     "--read"
#define MODEIOTEST_ARG_FILESIZE                 "--filesize"
#define MODEIOTEST_ARG_RECORDSIZE               "--recordsize"
#define MODEIOTEST_ARG_INTERMEDIATERESULTSECS   "--intermediate"


int ModeIOTest::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();

   std::string mgmtHost = app->getConfig()->getSysMgmtdHost();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   bool registerRes;
   bool unregisterRes;

   StringMapIter iter;

   // check privileges
   if(!ModeHelper::checkRootPrivileges() )
      return APPCODE_RUNTIME_ERROR;

   // check arguments

   bool useMountedPath = true;
   iter = cfg->find(MODEIOTEST_ARG_UNMOUNTEDPATH);
   if(iter != cfg->end() )
   {
      useMountedPath = false;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOTEST_ARG_FILESIZE);
   if(iter != cfg->end() )
   {
      this->cfgFilesize = UnitTk::strHumanToInt64(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOTEST_ARG_RECORDSIZE);
   if(iter != cfg->end() )
   {
      this->cfgRecordsize = UnitTk::strHumanToInt64(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOTEST_ARG_INTERMEDIATERESULTSECS);
   if(iter != cfg->end() )
   {
      this->cfgIntermediateSecs = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOTEST_ARG_WRITE);
   if(iter != cfg->end() )
   {
      this->cfgWrite = true;
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOTEST_ARG_READ);
   if(iter != cfg->end() )
   {
      this->cfgRead = true;
      cfg->erase(iter);
   }

   iter = cfg->begin();
   if(iter == cfg->end() )
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   std::string pathStr = iter->first;

   // find owner node
   Path path(pathStr);

   NodeHandle ownerNode;
   EntryInfo entryInfo;

   if(!ModeHelper::getEntryAndOwnerFromPath(path, useMountedPath,
         *metaNodes, app->getMetaRoot(), *metaBuddyGroupMapper,
         entryInfo, ownerNode))
   {
      return APPCODE_RUNTIME_ERROR;
   }

   // print some basic info
   std::cout << "Metadata node: " << ownerNode->getID() << std::endl;
   std::cout << "EntryID: " << entryInfo.getEntryID() << std::endl;

   registerRes = ModeHelper::registerNode();
   if(!registerRes)
   {
      std::cerr << "Unable to register at management server: " << mgmtHost << std::endl;
      goto cleanup_meta;
   }

   // write
   if(cfgWrite)
   {
      if(!writeTest(*ownerNode, &entryInfo) )
         goto cleanup_meta;
   }

   if(cfgRead)
   {
      if(!readTest(*ownerNode, &entryInfo) )
         goto cleanup_meta;
   }

   unregisterRes = ModeHelper::unregisterNode();
   if(!unregisterRes)
   {
      std::cerr << "Unable to unregister at management server: " << mgmtHost << std::endl;
      goto cleanup_meta;
   }

   retVal = APPCODE_NO_ERROR;

   // cleanup
cleanup_meta:
   return retVal;
}

void ModeIOTest::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                   Path to benchmark file." << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --write                  Perform write test." << std::endl;
   std::cout << "  --read                   Perform read test." << std::endl;
   std::cout << "  --filesize=<num>         " << std::endl;
   std::cout << "  --recordsize=<num>       " << std::endl;
   std::cout << "  --intermediate=<secs>    Print intermediate results." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode runs a streaming IO benchmark." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: n/a" << std::endl;
   std::cout << "  # beegfs-ctl --iotest <...>" << std::endl;
}

bool ModeIOTest::writeTest(Node& ownerNode, EntryInfo* entryInfo)
{
   bool retVal = true;
   unsigned intermediateMS = cfgIntermediateSecs * 1000;
   uint64_t lastIntermediateLen = 0;
   unsigned intermediateElapsedMS = 0;
   unsigned accessFlags = OPENFILE_ACCESS_WRITE;
   std::string fileHandleID;
   StripePattern* pattern = NULL;
   PathInfo pathInfo;

   bool openRes = openFile(ownerNode, entryInfo, accessFlags, &fileHandleID, &pattern, &pathInfo);
   if(!openRes)
      return false;

   boost::scoped_array<char> buf(new char[cfgRecordsize]);
   memset(buf.get(), 0xaa, cfgRecordsize);

   std::cout << "*****" << std::endl;
   std::cout << "WRITE recordsize: " << UnitTk::int64ToHumanStr(cfgRecordsize) <<
      ", filesize: " << UnitTk::int64ToHumanStr(cfgFilesize) << std::endl;
   std::cout << "*****" << std::endl;

   TimeFine totalTime;
   TimeFine intermediateTime;

   uint64_t len = 0;
   for( ; len < cfgFilesize; len += cfgRecordsize)
   {
      ssize_t writeRes = writefile(buf.get(), cfgRecordsize, len, fileHandleID, accessFlags,
            pattern, &pathInfo);
      if(writeRes < (ssize_t)cfgRecordsize)
      {
         std::cerr << "Error writing len " << cfgRecordsize << " at offset " << len << std::endl;
         retVal = false;
         break;
      }

      if(intermediateMS)
      {
         intermediateElapsedMS = intermediateTime.elapsedMS();
         if(intermediateElapsedMS >= intermediateMS)
         { // print intermediate result
            uint64_t bps =
               ( (len + cfgRecordsize - lastIntermediateLen) * 1000) / intermediateElapsedMS;
            uint64_t mbps = bps / (1024*1024);
            lastIntermediateLen = len + cfgRecordsize;

            std::cout << mbps << "MiB/s ";
            std::flush(std::cout);
            intermediateTime.setToNow();
         }
      }
   }

   // print final result
   unsigned totalElapsedMS = totalTime.elapsedMS();
   if(!retVal)
      std::cout << "Write result: n/a (reason: errors occurred)" << std::endl;
   else
   if(!totalElapsedMS)
      std::cout << "Write result: n/a (reason: time diff to small)" << std::endl;
   else
   {
      uint64_t totalBps = (len * 1000) / totalElapsedMS;
      uint64_t totalMiBps = totalBps / (1024*1024);
      std::cout << std::endl;
      std::cout << "Write result: " << totalMiBps << "MiB/s" << std::endl << std::endl;
   }


   bool closeRes = closeFile(ownerNode, fileHandleID, entryInfo, pattern->getStripeTargetIDs()->size() );
   if(!closeRes)
      retVal = false;


   SAFE_DELETE(pattern);

   return retVal;
}

bool ModeIOTest::readTest(Node& ownerNode, EntryInfo* entryInfo)
{
   bool retVal = true;
   unsigned intermediateMS = cfgIntermediateSecs * 1000;
   uint64_t lastIntermediateLen = 0;
   unsigned intermediateElapsedMS = 0;
   unsigned accessFlags = OPENFILE_ACCESS_READ;
   std::string fileHandleID;
   StripePattern* pattern = NULL;
   PathInfo pathInfo;

   bool openRes = openFile(ownerNode, entryInfo, accessFlags, &fileHandleID, &pattern, &pathInfo);
   if(!openRes)
      return false;

   boost::scoped_array<char> buf(new char[cfgRecordsize]);

   std::cout << "*****" << std::endl;
   std::cout << "READ recordsize: " << UnitTk::int64ToHumanStr(cfgRecordsize) <<
      ", filesize: " << UnitTk::int64ToHumanStr(cfgFilesize) << std::endl;
   std::cout << "*****" << std::endl;

   TimeFine totalTime;
   TimeFine intermediateTime;

   uint64_t len=0;
   for( ; len < cfgFilesize; len += cfgRecordsize)
   {
      ssize_t readRes = readfile(buf.get(), cfgRecordsize, len, fileHandleID, accessFlags, pattern,
         &pathInfo);
      if(readRes < (ssize_t)cfgRecordsize)
      {
         std::cerr << "Error reading len " << cfgRecordsize << " at offset " << len << std::endl;
         retVal = false;
         break;
      }

      if(intermediateMS)
      {
         intermediateElapsedMS = intermediateTime.elapsedMS();
         if(intermediateElapsedMS >= intermediateMS)
         { // print intermediate result
            uint64_t bps =
               ( (len + cfgRecordsize - lastIntermediateLen) * 1000) / intermediateElapsedMS;
            uint64_t mbps = bps / (1024*1024);
            lastIntermediateLen = len + cfgRecordsize;

            std::cout << mbps << "MiB/s ";
            std::flush(std::cout);
            intermediateTime.setToNow();
         }
      }
   }

   // print final result
   unsigned totalElapsedMS = totalTime.elapsedMS();
   if(!retVal)
      std::cout << "Read result: n/a (reason: errors occurred)" << std::endl;
   else
   if(!totalElapsedMS)
      std::cout << "Read result: n/a (reason: time diff to small)" << std::endl;
   else
   {
      uint64_t totalBps = (len * 1000) / totalElapsedMS;
      uint64_t totalMiBps = totalBps / (1024*1024);
      std::cout << std::endl;
      std::cout << "Read result: " << totalMiBps << "MiB/s" << std::endl << std::endl;
   }


   bool closeRes = closeFile(ownerNode, fileHandleID, entryInfo,
      pattern->getStripeTargetIDs()->size() );
   if(!closeRes)
      retVal = false;


   SAFE_DELETE(pattern);

   return retVal;
}

/**
 * @param openFlags OPENFILE_ACCESS_... flags
 */
bool ModeIOTest::openFile(Node& ownerNode, EntryInfo* entryInfo, unsigned openFlags,
   std::string* outFileHandleID, StripePattern** outPattern, PathInfo* outPathInfo)
{
   bool retVal = false;

   NumNodeID localNodeNumID = Program::getApp()->getLocalNode().getNumID();
   OpenFileRespMsg* respMsgCast;

   FhgfsOpsErr openRes;

   OpenFileMsg msg(localNodeNumID, entryInfo, openFlags);

   const auto respMsg = MessagingTk::requestResponse(ownerNode, msg, NETMSGTYPE_OpenFileResp);
   if (!respMsg)
   {
      std::cerr << "Communication error during open on server " << ownerNode.getID() << std::endl;
      goto err_cleanup;
   }

   respMsgCast = (OpenFileRespMsg*)respMsg.get();

   openRes = (FhgfsOpsErr)respMsgCast->getResult();
   if(openRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error during open: " <<
         openRes << std::endl;
      goto err_cleanup;
   }

   *outPattern = respMsgCast->getPattern().clone();
   *outFileHandleID = respMsgCast->getFileHandleID();
   outPathInfo->set(respMsgCast->getPathInfo() );

   retVal = true;

err_cleanup:
   return retVal;
}

bool ModeIOTest::closeFile(Node& ownerNode, std::string fileHandleID, EntryInfo* entryInfo,
   int maxUsedNodeIndex)
{
   bool retVal = false;

   NumNodeID localNodeNumID = Program::getApp()->getLocalNode().getNumID();
   CloseFileRespMsg* respMsgCast;

   FhgfsOpsErr closeRes;

   CloseFileMsg msg(localNodeNumID, fileHandleID, entryInfo, maxUsedNodeIndex);

   const auto respMsg = MessagingTk::requestResponse(ownerNode, msg, NETMSGTYPE_CloseFileResp);
   if (!respMsg)
   {
      std::cerr << "Communication error during close on server " << ownerNode.getID() << std::endl;
      goto err_cleanup;
   }

   respMsgCast = (CloseFileRespMsg*)respMsg.get();

   closeRes = (FhgfsOpsErr)respMsgCast->getValue();
   if(closeRes != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Server encountered an error during close: " << closeRes << std::endl;
      goto err_cleanup;
   }

   retVal = true;

err_cleanup:
   return retVal;
}

ssize_t ModeIOTest::writefile(const char *buf, size_t size, off_t offset, std::string fileHandleID,
   unsigned accessFlags, StripePattern* pattern, PathInfo* pathInfo)
{
   const char* logContext = "Write file remoting";
   App* app = Program::getApp();
   MultiWorkQueue* workQ = app->getWorkQueue();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   NumNodeID localNodeNumID = app->getLocalNode().getNumID();

   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   size_t numStripeNodes = targetIDs->size();
   Int64Vector nodeResults(numStripeNodes);
   Int64Vector expectedNodeResults(numStripeNodes);
   long long chunkSize = pattern->getChunkSize();

   size_t toBeWritten = size;
   off_t currentOffset = offset;

   SynchronizedCounter counter;
   WriteLocalFileWorkInfo writeInfo(localNodeNumID, targetMapper, storageNodes, &counter);

   while(toBeWritten)
   {
      unsigned firstNodeIndex = pattern->getStripeTargetIndex(currentOffset);

      unsigned currentNodeIndex = firstNodeIndex;
      unsigned numWorks = 0;

      counter.resetUnsynced();

      while(toBeWritten && (numWorks < numStripeNodes) )
      {
         size_t currentChunkSize = pattern->getChunkEnd(currentOffset) - currentOffset + 1;
         size_t currentWriteSize = BEEGFS_MIN(currentChunkSize, toBeWritten);
         size_t currentNodeLocalOffset =
            getNodeLocalOffset(currentOffset, chunkSize, numStripeNodes, currentNodeIndex);

         Work* work = new WriteLocalFileWork(fileHandleID.c_str(), &buf[size-toBeWritten],
            accessFlags, currentNodeLocalOffset, currentWriteSize, (*targetIDs)[currentNodeIndex],
            pathInfo, &nodeResults[currentNodeIndex], &writeInfo, false);
         workQ->addDirectWork(work);

         expectedNodeResults[currentNodeIndex] = currentWriteSize;

         // prepare for next loop
         currentOffset += currentWriteSize;
         toBeWritten -= currentWriteSize;
         numWorks++;
         currentNodeIndex = pattern->getStripeTargetIndex(currentOffset);
      }

      // wait for all work to be done
      counter.waitForCount(numWorks);

      // verify results
      for(unsigned i=0; i < numWorks; i++)
      {
         currentNodeIndex = (firstNodeIndex + i) % numStripeNodes;

         if(expectedNodeResults[currentNodeIndex] != nodeResults[currentNodeIndex] )
         { // something went wrong
            FhgfsOpsErr nodeError = (FhgfsOpsErr)-nodeResults[currentNodeIndex];
            LogContext(logContext).log(2, std::string("Storage node encountered an error: ") +
               StringTk::uintToStr( (*targetIDs)[currentNodeIndex] ) +
               std::string("; msg: ") + boost::lexical_cast<std::string>(nodeError));
            return -nodeError;
         }
      }
   }

   return size;
}

ssize_t ModeIOTest::readfile(char *buf, size_t size, off_t offset, std::string fileHandleID,
   unsigned accessFlags, StripePattern* pattern, PathInfo* pathInfo)
{
   const char* logContext = "Read file remoting";
   App* app = Program::getApp();
   MultiWorkQueue* workQ = app->getWorkQueue();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   NumNodeID localNodeNumID = app->getLocalNode().getNumID();

   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   size_t numStripeNodes = targetIDs->size();
   Int64Vector nodeResults(numStripeNodes);
   Int64Vector expectedNodeResults(numStripeNodes);
   long long chunkSize = pattern->getChunkSize();

   ssize_t usableReadSize = 0; // the amount of usable data that was received from the nodes

   size_t toBeRead = size;
   off_t currentOffset = offset;

   SynchronizedCounter counter;
   ReadLocalFileWorkInfo readInfo(localNodeNumID, targetMapper, storageNodes, &counter);

   while(toBeRead)
   {
      unsigned firstNodeIndex = pattern->getStripeTargetIndex(currentOffset);

      unsigned currentNodeIndex = firstNodeIndex;
      unsigned numWorks = 0;

      counter.resetUnsynced();

      while(toBeRead && (numWorks < numStripeNodes) )
      {
         size_t currentChunkSize = pattern->getChunkEnd(currentOffset) - currentOffset + 1;
         size_t currentReadSize = BEEGFS_MIN(currentChunkSize, toBeRead);
         size_t currentNodeLocalOffset =
            getNodeLocalOffset(currentOffset, chunkSize, numStripeNodes, currentNodeIndex);

         LOG_DEBUG(logContext, 5,
            std::string("toBeRead/bufOffset/nodeLocalOffset/readSize/nodeIndex: ") +
            StringTk::int64ToStr(toBeRead) + "/" + StringTk::int64ToStr(size-toBeRead) + "/" +
            StringTk::int64ToStr(currentNodeLocalOffset) + "/" +
            StringTk::int64ToStr(currentReadSize) + "/" +
            StringTk::uintToStr(currentNodeIndex) + "/" );

         Work* work = new ReadLocalFileV2Work(fileHandleID.c_str(), &buf[size-toBeRead],
            accessFlags, currentNodeLocalOffset, currentReadSize, (*targetIDs)[currentNodeIndex],
            pathInfo, &nodeResults[currentNodeIndex], &readInfo, false);
         workQ->addDirectWork(work);

         expectedNodeResults[currentNodeIndex] = currentReadSize;

         // prepare for next loop
         currentOffset += currentReadSize;
         toBeRead -= currentReadSize;
         numWorks++;
         currentNodeIndex = pattern->getStripeTargetIndex(currentOffset);
      }

      // wait for all work to be done
      counter.waitForCount(numWorks);

      // verify results
      for(unsigned i=0; i < numWorks; i++)
      {
         currentNodeIndex = (firstNodeIndex + i) % numStripeNodes;

         if(nodeResults[currentNodeIndex] < expectedNodeResults[currentNodeIndex] )
         { // check end of file or error
            if(nodeResults[currentNodeIndex] >= 0)
            { // we have an end of file here
               return usableReadSize + nodeResults.at(currentNodeIndex);
            }
            else
            { // error occurred
               FhgfsOpsErr nodeError = (FhgfsOpsErr)-nodeResults[currentNodeIndex];
               LogContext(logContext).log(2, std::string("Storage node encountered an error: ") +
                  StringTk::uintToStr( (*targetIDs)[currentNodeIndex] ) +
                  std::string("; msg: ") + boost::lexical_cast<std::string>(nodeError));
               return -nodeError;
            }
         }

         usableReadSize += nodeResults[currentNodeIndex];
      }

   }

   return size;
}

long long ModeIOTest::getNodeLocalOffset(long long pos, long long chunkSize, size_t numTargets,
   size_t stripeNodeIndex)
{
   long long posModChunkSize = (pos % chunkSize);
   long long chunkStart = pos - posModChunkSize - (stripeNodeIndex*chunkSize);

   return ( (chunkStart / numTargets) + posModChunkSize);
}

void ModeIOTest::printPattern(StripePattern* pattern)
{
   const UInt16Vector& stripeTargetIDs = *pattern->getStripeTargetIDs();

   std::cout << "Stripe pattern details:" << std::endl;
   std::cout << "+ Chunksize: " << UnitTk::int64ToHumanStr(pattern->getChunkSize() ) <<
      std::endl;
   std::cout << "+ Number of storage targets: " <<
      "actual: " << stripeTargetIDs.size() << "; " <<
      "desired: " << pattern->getDefaultNumTargets() <<
      std::endl;

   if (!stripeTargetIDs.empty())
   {
      std::cout << "+ Storage nodes: " << std::endl;

      UInt16VectorConstIter iter = stripeTargetIDs.begin();
      for( ; iter != stripeTargetIDs.end(); iter++)
      {
         std::cout << "  + " << *iter << std::endl;
      }
   }
}
