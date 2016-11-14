#include <program/Program.h>
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperStat.h>
#include "StatMsgEx.h"


bool StatMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "StatMsgEx incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a StatMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   App* app = Program::getApp();
   std::string localNodeID = app->getLocalNode()->getID();
   EntryInfo* entryInfo = this->getEntryInfo();

   LOG_DEBUG(logContext, 5, "ParentID: " + entryInfo->getParentEntryID() +
      " EntryID: " + entryInfo->getEntryID() +
      " EntryType: " + StringTk::intToStr(entryInfo->getEntryType()));
   
   StatData statData;
   FhgfsOpsErr statRes;
   
   if(entryInfo->getParentEntryID().empty() || (entryInfo->getEntryID() == META_ROOTDIR_ID_STR) )
   { // special case: stat for root directory
      statRes = statRoot(statData);

   }
   else
   {
      statRes = MsgHelperStat::stat(entryInfo, true, statData);
   }

   LOG_DEBUG(logContext, 4, std::string("statRes: ") + FhgfsOpsErrTk::toErrString(statRes) );

   StatRespMsg respMsg(statRes, statData);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_STAT);

   return true;      
}

FhgfsOpsErr StatMsgEx::statRoot(StatData& outStatData)
{
   DirInode* rootDir = Program::getApp()->getRootDir();

   uint16_t localNodeID = Program::getApp()->getLocalNode()->getNumID();

   if(localNodeID != rootDir->getOwnerNodeID() )
   {
      return FhgfsOpsErr_NOTOWNER;
   }
   
//   size_t subdirsSize = rootDir->getSubdirs()->getSize();
//
//   outStatData->mode = S_IFDIR | 0777;
//   outStatData->size = rootDir->getFiles()->getSize() + subdirsSize;
//   outStatData->nlink = 2 + subdirsSize; // 2 + numSubDirs by definition

   rootDir->getStatData(outStatData);

   
   return FhgfsOpsErr_SUCCESS;
}

