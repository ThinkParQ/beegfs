#include "FindLinkOwnerMsgEx.h"

#include <program/Program.h>

bool FindLinkOwnerMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock, char* respBuf,
   size_t bufLen, HighResolutionStats* stats) throw (SocketException)
{

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG("FindLinkOwnerMsg incoming", 4,
      std::string("Received a FindLinkOwnerMsg from: ") + peer);

   MetaStore *metaStore = Program::getApp()->getMetaStore();
   std::string entryID = this->getEntryID();

   int result = 1;
   std::string parentEntryID;
   uint16_t parentNodeID = 0;

   FileInode *file = NULL;
   DirInode *dir = NULL;
   // we don't know if the ID belongs to a file or a directory, so we try the file first and if that
   // does not work, we try directory

   /* FIXME Bernd / Christian: referenceFile now requires parentEntryID, actually we need the
    *                          entire EntryInfo to be able to use referenceFile()
    */

   file = metaStore->referenceLoadedFile(parentEntryID, entryID);
   if (file != NULL)
   {
      file->getParentInfo(&parentEntryID, &parentNodeID);
      result = 0;
      metaStore->releaseFile(parentEntryID, file);
      goto send_response;
   }
   dir = metaStore->referenceDir(entryID, true);
   if (dir != NULL)
   {
      dir->getParentInfo(&parentEntryID, &parentNodeID);
      result = 0;
      metaStore->releaseDir(entryID);
      goto send_response;
   }

send_response: 
   FindLinkOwnerRespMsg respMsg(result, parentNodeID, parentEntryID);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;
}

