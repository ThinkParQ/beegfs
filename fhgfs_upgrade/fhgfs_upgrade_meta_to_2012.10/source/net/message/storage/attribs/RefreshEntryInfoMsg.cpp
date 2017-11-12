#include <common/net/message/storage/attribs/RefreshEntryInfoRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperStat.h>
#include <common/storage/EntryInfo.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include "RefreshEntryInfoMsgEx.h"


bool RefreshEntryInfoMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("RefreshEntryInfoMsgEx incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a RefreshEntryInfoMsg from: ") + peer);

   LOG_DEBUG("RefreshEntryInfoMsgEx::processIncoming", 5, std::string("ParentID: ") +
      this->getEntryInfo()->getParentEntryID() + " EntryID: " +
      this->getEntryInfo()->getEntryID() );

   FhgfsOpsErr refreshInfoRes;


   if(this->getEntryInfo()->getParentEntryID().length() == 0) // FIXME Bernd: Testme
   { // special case: get info for root directory
      refreshInfoRes = refreshInfoRoot();
   }
   else
   {
      refreshInfoRes = refreshInfoRec(this->getEntryInfo() );
   }


   RefreshEntryInfoRespMsg respMsg(refreshInfoRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_REFRESHENTRYINFO);

   return true;
}


/**
 * @param outPattern StripePattern clone (in case of success), that must be deleted by the caller
 */
FhgfsOpsErr RefreshEntryInfoMsgEx::refreshInfoRec(EntryInfo* entryInfo)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   DirInode* dir = metaStore->referenceDir(entryInfo->getEntryID(), true);
   if(dir)
   { // entry is a directory
      dir->refreshMetaInfo();
      metaStore->releaseDir(entryInfo->getEntryID() );
      return FhgfsOpsErr_SUCCESS;
   }

   // not a dir => try file

   return MsgHelperStat::refreshDynAttribs(entryInfo, true);
}

FhgfsOpsErr RefreshEntryInfoMsgEx::refreshInfoRoot()
{
   DirInode* rootDir = Program::getApp()->getRootDir();

   uint16_t localNodeID = Program::getApp()->getLocalNode()->getNumID();

   if(localNodeID != rootDir->getOwnerNodeID() )
   {
      return FhgfsOpsErr_NOTOWNER;
   }

   rootDir->refreshMetaInfo();

   return FhgfsOpsErr_SUCCESS;
}
