#include <common/toolkit/serialization/Serialization.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <nodes/NodeStoreEx.h>
#include <program/Program.h>

#include "SessionLocalFile.h"
#include <storage/ChunkStore.h>

void SessionLocalFile::serializeNodeID(SessionLocalFile* obj, Deserializer& des)
{
   uint16_t mirrorNodeID;

   des % mirrorNodeID;
   if (unlikely(!des.good()))
      return;

   if(mirrorNodeID)
   {
      NodeStoreServersEx* nodeStore = Program::getApp()->getStorageNodes();
      auto node = nodeStore->referenceNode(NumNodeID(mirrorNodeID) );

      if(!node)
         des.setBad();

      obj->mirrorNode = node;
   }
}

/**
 * Open a chunkFile for this session
 *
 * @param quotaInfo may be NULL if not isWriteOpen (i.e. if the file will not be created).
 * @param isWriteOpen if set to true, the file will be created if it didn't exist.
 */
FhgfsOpsErr SessionLocalFile::openFile(int targetFD, PathInfo *pathInfo,
   bool isWriteOpen, SessionQuotaInfo* quotaInfo)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   Logger* log = app->getLogger();
   const char* logContext = "SessionLocalFile (open)";


   if(this->fileDescriptor != -1) // no lock here as optimization, with lock below
      return FhgfsOpsErr_SUCCESS; // file already open


   SafeMutexLock safeMutex(&this->sessionMutex); // L O C K


   if(this->fileDescriptor != -1)
   {
      // file already open (race with another thread) => nothing more to do here
   }
   else
   {  // open chunk file (and create dir if necessary)...

      std::string entryID = getFileID();
      Path chunkDirPath;
      std::string chunkFilePathStr;
      bool hasOrigFeature = pathInfo->hasOrigFeature();

      StorageTk::getChunkDirChunkFilePath(pathInfo, entryID, hasOrigFeature, chunkDirPath,
         chunkFilePathStr);

      ChunkStore* chunkDirStore = app->getChunkDirStore();

      int fd = -1;

      if (isWriteOpen)
      {  // chunk needs to be created if not exists
         int openFlags = O_CREAT | this->openFlags;

         FhgfsOpsErr openChunkRes = chunkDirStore->openChunkFile(
            targetFD, &chunkDirPath, chunkFilePathStr, hasOrigFeature, openFlags, &fd, quotaInfo);

         // fix chunk path permissions
         if (unlikely(openChunkRes == FhgfsOpsErr_NOTOWNER && quotaInfo->useQuota) )
         {
            // it already logs a message, so need to further check this ret value
            chunkDirStore->chmodV2ChunkDirPath(targetFD, &chunkDirPath, entryID);

            openChunkRes = chunkDirStore->openChunkFile(
               targetFD, &chunkDirPath, chunkFilePathStr, hasOrigFeature, openFlags, &fd,
               quotaInfo);
         }

         if (openChunkRes != FhgfsOpsErr_SUCCESS)
         {
            if (openChunkRes == FhgfsOpsErr_INTERNAL) // only log unhandled errors
               LogContext(logContext).logErr("Failed to open chunkFile: " + chunkFilePathStr);

            retVal = openChunkRes;
         }
      }
      else
      {  // just reading the file, no create
         mode_t openMode = S_IRWXU|S_IRWXG|S_IRWXO;

         fd = MsgHelperIO::openat(targetFD, chunkFilePathStr.c_str(), this->openFlags,
            openMode);

         if(fd == -1)
         { // not exists or error
            int errCode = errno;

            // we ignore ENOENT (file does not exist), as that's not an error
            if(errCode != ENOENT)
            {
               log->logErr(logContext, "Unable to open file: " + chunkFilePathStr + ". " +
                  "SysErr: " + System::getErrString(errCode) );

               retVal = FhgfsOpsErrTk::fromSysErr(errCode);

            }
         }
      }

      // prepare session data...
      setFDUnlocked(fd);
      setOffsetUnlocked(0);

      log->log(Log_DEBUG, logContext, "File created. ID: " + getFileID() );
   }


   safeMutex.unlock(); // U N L O C K

   return retVal;
}

/**
 * To avoid races with other writers in same session, apply new mirrorNode only if it was
 * NULL before. Otherwise release given mirrorNode and return the existing one.
 *
 * @mirrorNode will be released if a mirrorNode was set already.
 * @return existing mirrorNode if it was not NULL, given mirrorNode otherwise.
 */
NodeHandle SessionLocalFile::setMirrorNodeExclusive(NodeHandle mirrorNode)
{
   SafeMutexLock safeMutex(&this->sessionMutex); // L O C K

   if (!this->mirrorNode)
      this->mirrorNode = mirrorNode;

   NodeHandle retVal = this->mirrorNode;

   safeMutex.unlock(); // U N L O C K

   return retVal;
}

