#include <common/toolkit/serialization/Serialization.h>
#include <common/storage/quota/ExceededQuotaStore.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <program/Program.h>
#include <storage/ChunkStore.h>

#include "SessionLocalFile.h"

bool SessionLocalFile::Handle::close()
{
   if (!fd.valid())
      return true;

   if (const int err = fd.close())
   {
      LOG(GENERAL, ERR, "Unable to close local file.", sysErr(err), id);
      return false;
   }
   else
   {
      LOG(GENERAL, DEBUG, "Local file closed.", id);
      return true;
   }
}

void SessionLocalFile::serializeNodeID(SessionLocalFile* obj, Deserializer& des)
{
   uint16_t mirrorNodeID;

   des % mirrorNodeID;
   if (unlikely(!des.good()))
      return;

   if(mirrorNodeID)
   {
      NodeStoreServers* nodeStore = Program::getApp()->getStorageNodes();
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
FhgfsOpsErr SessionLocalFile::openFile(int targetFD, const PathInfo* pathInfo,
   bool isWriteOpen, const SessionQuotaInfo* quotaInfo)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   App* app = Program::getApp();
   Logger* log = Logger::getLogger();
   const char* logContext = "SessionLocalFile (open)";


   if (handle->fd.valid()) // no lock here as optimization, with lock below
      return FhgfsOpsErr_SUCCESS; // file already open


   std::lock_guard<Mutex> const lock(sessionMutex);


   if (handle->fd.valid())
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

         const ExceededQuotaStorePtr exceededQuotaStore =
               app->getExceededQuotaStores()->get(getTargetID());

         FhgfsOpsErr openChunkRes = chunkDirStore->openChunkFile(
            targetFD, &chunkDirPath, chunkFilePathStr, hasOrigFeature, openFlags, &fd, quotaInfo,
            exceededQuotaStore);

         // fix chunk path permissions
         if (unlikely(openChunkRes == FhgfsOpsErr_NOTOWNER && quotaInfo->useQuota) )
         {
            // it already logs a message, so need to further check this ret value
            chunkDirStore->chmodV2ChunkDirPath(targetFD, &chunkDirPath, entryID);

            openChunkRes = chunkDirStore->openChunkFile(
               targetFD, &chunkDirPath, chunkFilePathStr, hasOrigFeature, openFlags, &fd,
               quotaInfo, exceededQuotaStore);
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
               LOG(SESSIONS, ERR, "Unable to open file.",
                     ("Chunk File Path", chunkFilePathStr),
                     ("SysErr", System::getErrString(errCode)));

               retVal = FhgfsOpsErrTk::fromSysErr(errCode);

            }
         }
      }

      // prepare session data...
      handle->fd = FDHandle(fd);
      offset = 0;

      log->log(Log_DEBUG, logContext, "File created. ID: " + getFileID() );
   }

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
   std::lock_guard<Mutex> const lock(sessionMutex);

   if (!this->mirrorNode)
      this->mirrorNode = mirrorNode;

   return this->mirrorNode;
}

