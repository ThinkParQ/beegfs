#include "RecreateFsIDsMsgEx.h"

#include <common/fsck/FsckDirEntry.h>
#include <program/Program.h>

bool RecreateFsIDsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
   size_t bufLen, HighResolutionStats* stats)
{
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG("RecreateFsIDsMsg incoming", 4,
      std::string("Received a RecreateFsIDsMsg from: ") + peer);

   LogContext log("RecreateFsIDsMsgEx");

   FsckDirEntryList entries;
   FsckDirEntryList failedEntries;

   this->parseEntries(&entries);

   for ( FsckDirEntryListIter iter = entries.begin(); iter != entries.end(); iter++ )
   {
      std::string parentID = iter->getParentDirID();
      std::string entryName = iter->getName();
      std::string entryID = iter->getID();

      std::string dirEntryPath = MetaStorageTk::getMetaDirEntryPath(
         Program::getApp()->getStructurePath()->getPathAsStrConst(), parentID);

      std::string dirEntryIDFilePath = MetaStorageTk::getMetaDirEntryIDPath(dirEntryPath) + "/"
         + entryID;

      std::string dirEntryNameFilePath = dirEntryPath + "/" + entryName;

      // delete the old dentry-by-id file link (if one existed)
      int removeRes = unlink(dirEntryIDFilePath.c_str());

      if ( (removeRes) && (errno != ENOENT) )
      {
         log.logErr(
            "Failed to recreate dentry-by-id file for directory entry; ParentID: " + parentID
               + "; EntryName: " + entryName
               + " - Could not delete old, faulty dentry-by-id file link");
         failedEntries.push_back(*iter);
         continue;
      }

      // link a new one
      int linkRes = link(dirEntryNameFilePath.c_str(), dirEntryIDFilePath.c_str());

      if ( linkRes )
      {
         log.logErr(
            "Failed to recreate dentry-by-id file for directory entry; ParentID: " + parentID
               + "; EntryName: " + entryName + " - File could not be linked");
         failedEntries.push_back(*iter);
         continue;
      }
   }

   RecreateFsIDsRespMsg respMsg(&failedEntries);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;
}
