#include <common/net/message/storage/attribs/GetEntryInfoRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include "GetEntryInfoMsgEx.h"


bool GetEntryInfoMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "GetEntryInfoMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a GetEntryInfoMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   EntryInfo* entryInfo = this->getEntryInfo();
   LOG_DEBUG(logContext, 5, std::string("ParentEntryID: ") + entryInfo->getParentEntryID() +
      " entryID: " + entryInfo->getParentEntryID() );
   
   StripePattern* pattern = NULL;   
   FhgfsOpsErr getInfoRes;

   
   if(entryInfo->getEntryID().length() == 0 ) // FIXME Bernd: Test
   { // special case: get info for root directory
      getInfoRes = getInfoRoot(&pattern);
   }
   else
   {
      getInfoRes = getInfoRec(entryInfo, &pattern);
   }
   
   
   if(getInfoRes != FhgfsOpsErr_SUCCESS)
   { // error occurred => create a dummy pattern
      UInt16Vector dummyStripeNodes;
      pattern = new Raid0Pattern(1, dummyStripeNodes);
   }

   
   GetEntryInfoRespMsg respMsg(getInfoRes, pattern);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   // cleanup
   delete(pattern);

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_GETENTRYINFO);

   return true;      
}


/**
 * @param outPattern StripePattern clone (in case of success), that must be deleted by the caller
 */
FhgfsOpsErr GetEntryInfoMsgEx::getInfoRec(EntryInfo* entryInfo, StripePattern** outPattern)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   
   if (entryInfo->getEntryType() == DirEntryType_DIRECTORY)
   {
      // entry is a directory
      DirInode* dir = metaStore->referenceDir(entryInfo->getEntryID(), true);
      if(dir)
      {
         *outPattern = dir->getStripePatternClone();
         metaStore->releaseDir(entryInfo->getEntryID() );
         return FhgfsOpsErr_SUCCESS;
      }
   }
   else
   {
      // entry is a inode
      FileInode* inode = metaStore->referenceFile(entryInfo);
      if(inode)
      {
         *outPattern = inode->getStripePattern()->clone();
         metaStore->releaseFile(entryInfo->getParentEntryID(), inode);
         return FhgfsOpsErr_SUCCESS;
      }
   }
   
   return FhgfsOpsErr_PATHNOTEXISTS;
}

/**
 * @param outPattern StripePattern clone (in case of success), that must be deleted by the caller
 */
FhgfsOpsErr GetEntryInfoMsgEx::getInfoRoot(StripePattern** outPattern)
{
   DirInode* rootDir = Program::getApp()->getRootDir();

   uint16_t localNodeID = Program::getApp()->getLocalNode()->getNumID();

   if(localNodeID != rootDir->getOwnerNodeID() )
   {
      return FhgfsOpsErr_NOTOWNER;
   }
   
   *outPattern = rootDir->getStripePatternClone();
   
   return FhgfsOpsErr_SUCCESS;
}
