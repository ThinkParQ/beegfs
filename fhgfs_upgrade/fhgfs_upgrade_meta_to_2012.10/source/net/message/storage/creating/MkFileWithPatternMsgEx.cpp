#include <program/Program.h>
#include <common/net/message/storage/creating/MkFileWithPatternRespMsg.h>
#include <common/net/message/storage/creating/MkLocalFileMsg.h>
#include <common/net/message/storage/creating/MkLocalFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include "MkFileWithPatternMsgEx.h"

// Called from fhgfs-ctl (online_cfg) to create a file on a specific node.

bool MkFileWithPatternMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "MkFileWithPatternMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a MkFileWithPatternMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   EntryInfo* parentInfo = this->getParentInfo();
   std::string newFileName = this->getNewFileName();
   EntryInfo newEntryInfo;

   FhgfsOpsErr mkRes = mkFile(parentInfo, newFileName, &newEntryInfo);

   MkFileWithPatternRespMsg respMsg(mkRes, &newEntryInfo );
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_MKFILE);

   return true;
}


/**
 * @param dir current directory
 * @param currentDepth 1-based path depth
 */
FhgfsOpsErr MkFileWithPatternMsgEx::mkFile(EntryInfo* parentInfo, std::string newName,
   EntryInfo* outEntryInfo)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   // reference parent
   DirInode* dir = metaStore->referenceDir(parentInfo->getEntryID(), true);
   if(!dir)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // create meta file
   retVal = mkMetaFile(dir, newName, outEntryInfo);

   // clean-up
   metaStore->releaseDir(parentInfo->getEntryID() );

   return retVal;
}

/**
 * Create an inode and directory-entry
 */
FhgfsOpsErr MkFileWithPatternMsgEx::mkMetaFile(DirInode* dir, std::string fileName,
   EntryInfo* outEntryInfo)
{
   // note: to guarantee amtomicity of file creation (from the view of a client), we have
   //       to create the inode first and insert the directory entry afterwards

   LogContext log("MkFileWithPatternMsg::mkMetaFile");

   App* app = Program::getApp();
   TargetCapacityPools* capacityPools = app->getStorageCapacityPools();
   MetaStore* metaStore = Program::getApp()->getMetaStore();


   // check stripe pattern

   StripePattern* stripePattern = createPattern();
   UInt16Vector stripeTargets;
   UInt16List preferredTargets(stripePattern->getStripeTargetIDs()->begin(),
      stripePattern->getStripeTargetIDs()->end() );

   // choose targets based on preference setting of given stripe pattern
   capacityPools->chooseStorageTargets(
      stripePattern->getDefaultNumTargets(), &preferredTargets, &stripeTargets);


   // check availability of stripe nodes
   if(stripeTargets.empty() || (stripeTargets.size() < stripePattern->getMinNumTargets() ) )
   {
      log.logErr(std::string("No (or not enough) storage targets available") );
      delete(stripePattern);

      return FhgfsOpsErr_INTERNAL;
   }

   // swap given preferred targets of stripe pattern with chosen targets
   stripePattern->getStripeTargetIDsModifyable()->swap(stripeTargets);

   MkFileDetails mkDetails(fileName, this->getUserID(), this->getGroupID(), this->getMode() );

   DentryInodeMeta dentryInodeMeta;

   FhgfsOpsErr makeRes = metaStore->mkMetaFile(dir, &mkDetails, NULL, stripePattern,
      outEntryInfo, &dentryInodeMeta); // deletes/frees stripePattern

   return makeRes;
}


