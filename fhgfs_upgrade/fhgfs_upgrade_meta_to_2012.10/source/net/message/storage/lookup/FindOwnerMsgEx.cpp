#include <program/Program.h>
#include <common/net/message/storage/lookup/FindOwnerRespMsg.h>
#include "FindOwnerMsgEx.h"

// FIXME Bernd: return DentryInodeMeta inodeMetaData as well?
bool FindOwnerMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "FindOwnerMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, Log_DEBUG, std::string("Received a FindOwnerMsg from: ") + peer);
   #endif // FHGFS_DEBUG
   
   Path path;
   parsePath(&path);

   LOG_DEBUG(logContext, Log_SPAM, "Path: '" + path.getPathAsStr() + "'");

   EntryOwnerInfo ownerInfo;
   FhgfsOpsErr findRes;
   
   if(!getSearchDepth() )
   { // looking for the root node
      uint16_t rootNodeID = Program::getApp()->getRootDir()->getOwnerNodeID();
      
      if(rootNodeID)
      {
         ownerInfo.update(0, rootNodeID, "", META_ROOTDIR_ID_STR, "/", DirEntryType_DIRECTORY, 0);
         
         findRes = FhgfsOpsErr_SUCCESS;
      }
      else
      { // we don't know the root node
         findRes = FhgfsOpsErr_INTERNAL;
      }
   }
   else
   { // a normal path lookup
      findRes = findOwner(&ownerInfo);
   }
   
   FindOwnerRespMsg respMsg(findRes, &ownerInfo);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_FINDOWNER);

   return true;
}


/**
 * Note: This does not work for finding the root dir owner (because it relies on the existence
 * of a parent dir).
 */
FhgfsOpsErr FindOwnerMsgEx::findOwner(EntryOwnerInfo* outInfo)
{
   Path path;
   parsePath(&path);

   FhgfsOpsErr findRes = FhgfsOpsErr_INTERNAL;
   
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   uint16_t localNodeID = app->getLocalNode()->getNumID();
   
   unsigned searchDepth = this->getSearchDepth();
   unsigned currentDepth = this->getCurrentDepth();
   EntryInfo *currentEntryInfo = this->getEntryInfo();
   std::string currentEntryID = currentEntryInfo->getEntryID();
   
   const StringList* pathElems = path.getPathElems();
   StringListConstIter pathIter = pathElems->begin();
   
   EntryInfo subInfo;
   DirEntryType subEntryType;
   
   // move iterator to corresponding path element
   for(unsigned i=0; i < currentDepth; i++)
      pathIter++;
   
   // search
   while(currentDepth < searchDepth)
   {
      DirInode* currentDir = metaStore->referenceDir(currentEntryID, true);
      if(!currentDir)
      { // someone said we own the dir with this ID, but we do not => it was deleted
         // note: this might also be caused by a change of ownership, but a feature like that is
         // currently not implemented
         return (findRes == FhgfsOpsErr_SUCCESS) ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_PATHNOTEXISTS;
      }
      
      DentryInodeMeta inodeMetaData;
      subEntryType = currentDir->getEntryInfo(*pathIter, &subInfo, &inodeMetaData);
      
      if(DirEntryType_ISDIR(subEntryType) || DirEntryType_ISFILE(subEntryType) )
      { // next entry exists and is a dir or a file
         currentDepth++;
         
         subInfo.getOwnerInfo(currentDepth, outInfo);
         
         findRes = FhgfsOpsErr_SUCCESS;
      }
      else
      { // entry does not exist
         findRes = FhgfsOpsErr_PATHNOTEXISTS;
      }
      
      metaStore->releaseDir(currentEntryID);
      
      if(findRes != FhgfsOpsErr_SUCCESS)
         break;
      
      if(outInfo->getOwnerNodeID() != localNodeID) // next entry owned by other node
         break;
      
      // prepare for next round
      currentEntryID = outInfo->getEntryID();
      pathIter++;
   }

   return findRes;
}
