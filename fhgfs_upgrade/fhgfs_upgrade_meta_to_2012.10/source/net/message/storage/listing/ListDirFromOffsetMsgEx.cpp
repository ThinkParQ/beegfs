#include <program/Program.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include "ListDirFromOffsetMsgEx.h"


bool ListDirFromOffsetMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "ListDirFromOffsetMsgEx incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a ListDirFromOffsetMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   EntryInfo* entryInfo = this->getEntryInfo();

   LOG_DEBUG(logContext, 5,
      std::string("serverOffset: ") + StringTk::int64ToStr(getServerOffset() ) + " " +
      std::string("maxOutNames: ") + StringTk::int64ToStr(getMaxOutNames() )   + " " +
      std::string("parentEntryID: ") + entryInfo->getParentEntryID()           + " " +
      std::string("entryID: ")       + entryInfo->getEntryID() );
   
   // FIXME Bernd: Test: Set on client side...
   // if(entryInfo->getParentEntryID().length() == 0 )
   //   path.appendElem(META_ROOTDIR_ID_STR); // special case: root dir

   StringList names;
   IntList entryTypes; // optional (depending on retrieveEntryTypes value)
   int64_t newServerOffset = getServerOffset(); // init to something useful

   FhgfsOpsErr listRes = listDirRec(entryInfo, &names, &entryTypes, &newServerOffset);
   
   LOG_DEBUG(logContext, 5,
      std::string("newServerOffset: ") + StringTk::int64ToStr(newServerOffset) + " " +
      std::string("names.size: ") + StringTk::int64ToStr(names.size() ) );
   
   ListDirFromOffsetRespMsg respMsg(listRes, &names, &entryTypes, newServerOffset);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_READDIR);

   return true;      
}

FhgfsOpsErr ListDirFromOffsetMsgEx::listDirRec(EntryInfo* entryInfo, StringList* outNames,
   IntList* outEntryTypes, int64_t* outNewOffset)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   // reference dir
   DirInode* dir = metaStore->referenceDir(entryInfo->getEntryID(), true);
   if(!dir)
      return FhgfsOpsErr_PATHNOTEXISTS;
   
   // query contents
   ListIncExOutArgs outArgs =
   {
      outNames,
      outEntryTypes,
      outNewOffset
   };

   FhgfsOpsErr listRes = dir->listIncrementalEx(getServerOffset(), getIncrementalOffset(),
      getMaxOutNames(), outArgs);

   // clean-up
   metaStore->releaseDir(entryInfo->getEntryID() );
   
   return listRes;
}

