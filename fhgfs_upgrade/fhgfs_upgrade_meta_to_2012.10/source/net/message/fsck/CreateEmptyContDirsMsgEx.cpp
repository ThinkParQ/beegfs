#include "CreateEmptyContDirsMsgEx.h"
#include <program/Program.h>

bool CreateEmptyContDirsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   const char* logContext = "CreateEmptyContDirsMsg incoming";
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG(logContext, 4, std::string("Received a CreateEmptyContDirsMsg from: ") + peer);

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   std::string metaDataPath = app->getStructurePath()->getPathAsStrConst();

   StringList directoryIDs;
   StringList failedIDs;
   this->parseDirIDs(&directoryIDs);

   for (StringListIter iter = directoryIDs.begin(); iter != directoryIDs.end(); iter++)
   {
      std::string contentsDirStr = MetaStorageTk::getMetaDirEntryPath( metaDataPath, *iter);

      // create contents directory
      int mkRes = mkdir(contentsDirStr.c_str(), 0755);

      // update the dir attribs
      std::string dirID = *iter;
      DirInode* dirInode = metaStore->referenceDir(dirID, true);

      if (!dirInode)
      {
         LogContext(logContext).logErr("Unable to reference directory ID: " + *iter);
         failedIDs.push_back(*iter);
         continue;
      }

      FhgfsOpsErr refreshRes = dirInode->refreshMetaInfo();

      if (refreshRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).log(Log_NOTICE, "Unable to refresh contents directory metadata: "
            + contentsDirStr + ". " + "SysErr: " + System::getErrString());
      }

      metaStore->releaseDir(dirID);

      if ( mkRes != 0 )
      { // error
         LogContext(logContext).logErr("Unable to create contents directory: " + contentsDirStr
            + ". " + "SysErr: " + System::getErrString());
         failedIDs.push_back(*iter);
      }
   }

   CreateEmptyContDirsRespMsg respMsg(&failedIDs);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;

}
