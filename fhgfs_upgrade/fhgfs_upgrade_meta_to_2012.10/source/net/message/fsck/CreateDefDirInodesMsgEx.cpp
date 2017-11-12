#include "CreateDefDirInodesMsgEx.h"
#include <program/Program.h>

bool CreateDefDirInodesMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG("CreateDefDirInodesMsg incoming", 4,
      std::string("Received a CreateDefDirInodesMsg from: ") + peer);

   LogContext log("CreateDefDirInodesMsg incoming");

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   uint16_t localNodeNumID = app->getLocalNode()->getNumID();

   StringList inodeIDs;
   StringList failedInodeIDs;
   FsckDirInodeList createdInodes;

   this->parseInodeIDs(&inodeIDs);

   for ( StringListIter iter = inodeIDs.begin(); iter != inodeIDs.end(); iter++ )
   {
      std::string inodeID = *iter;
      int mode = S_IFDIR | S_IRWXG | S_IRWXO;
      unsigned userID = 0; // root
      unsigned groupID = 0; // root
      uint16_t ownerNodeID = localNodeNumID;

      UInt16Vector stripeTargets;
      unsigned defaultChunkSize = cfg->getTuneDefaultChunkSize();
      unsigned defaultNumStripeTargets = cfg->getTuneDefaultNumStripeTargets();
      Raid0Pattern stripePattern(defaultChunkSize, stripeTargets, defaultNumStripeTargets);

      // we try to create a new directory inode, with default values
      DirInode dirInode(inodeID, mode, userID, groupID, ownerNodeID, stripePattern);
      if ( dirInode.storeAsReplacementFile(inodeID) == FhgfsOpsErr_SUCCESS )
      {
         // try to refresh the metainfo (maybe a .cont directory was already present)
         dirInode.refreshMetaInfo();

         StatData statData;
         dirInode.getStatData(statData);

         FsckDirInode fsckDirInode(inodeID, "", 0, localNodeNumID, statData.getFileSize(),
            statData.getNumHardlinks(), stripeTargets, FsckStripePatternType_RAID0, localNodeNumID);
         createdInodes.push_back(fsckDirInode);
      }
      else
         failedInodeIDs.push_back(inodeID);
   }

   CreateDefDirInodesRespMsg respMsg(&failedInodeIDs, &createdInodes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;

}
