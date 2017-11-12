#include "DeleteDirEntriesMsgEx.h"

#include <common/fsck/FsckDirEntry.h>
#include <program/Program.h>

bool DeleteDirEntriesMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG("DeleteDirEntriesMsg incoming", 4,
      std::string("Received a DeleteDirEntriesMsg from: ") + peer);
   LogContext log("DeleteDirEntriesMsgEx");

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FsckDirEntryList entries;
   FsckDirEntryList failedEntries;

   this->parseEntries(&entries);

   for ( FsckDirEntryListIter iter = entries.begin(); iter != entries.end(); iter++ )
   {
      std::string parentID = iter->getParentDirID();
      std::string entryName = iter->getName();
      FsckDirEntryType dirEntryType = iter->getEntryType();

      DirInode* parentDirInode = metaStore->referenceDir(parentID, true);

      if (!parentDirInode)
      {
         log.log(3,"Failed to delete directory entry; ParentID: " + parentID + "; EntryName: " +
            entryName + " - ParentID does not exist");
         failedEntries.push_back(*iter);
         continue;
      }

      FhgfsOpsErr unlinkRes;

      if (FsckDirEntryType_ISDIR(dirEntryType))
         unlinkRes = parentDirInode->removeDir(entryName, NULL);
      else
         unlinkRes = parentDirInode->unlinkDirEntry(entryName, NULL, true, NULL);

      metaStore->releaseDir(parentID);

      if (unlinkRes != FhgfsOpsErr_SUCCESS )
      {
         log.logErr("Failed to delete directory entry; ParentID: " + parentID + "; EntryName: " +
            entryName + "; Err: " + FhgfsOpsErrTk::toErrString(unlinkRes));
          failedEntries.push_back(*iter);
      }
   }

   DeleteDirEntriesRespMsg respMsg(&failedEntries);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   return true;
}
