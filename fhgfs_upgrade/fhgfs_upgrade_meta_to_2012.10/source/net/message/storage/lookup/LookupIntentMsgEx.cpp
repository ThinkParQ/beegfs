#include <program/Program.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/SessionTk.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <net/msghelpers/MsgHelperOpen.h>
#include <net/msghelpers/MsgHelperStat.h>
#include "LookupIntentMsgEx.h"


bool LookupIntentMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   const char* logContext = "LookupIntentMsg incoming";

   #ifdef FHGFS_DEBUG
      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a LookupIntentMsg from: ") + peer);
   #endif

   App* app = Program::getApp();

   std::string parentEntryID = getParentInfo()->getEntryID();

   FhgfsOpsErr createRes = FhgfsOpsErr_INTERNAL;

   std::string entryName = getEntryName();

   LOG_DEBUG(logContext, 5, "parentID: '" + parentEntryID + "' " +
      "entryName: '" + entryName + "'");


   // (note: following objects must be at the top-level stack frame for response serialization)
   std::string fileHandleID;
   Raid0Pattern dummyPattern(1, UInt16Vector() );
   EntryInfo diskEntryInfo;

   int createFlag = getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE;
   DentryInodeMeta inodeData;
   FhgfsOpsErr lookupRes;
   LookupIntentRespMsg respMsg(FhgfsOpsErr_INTERNAL);

   // sanity checks
   if (unlikely (parentEntryID.empty() || entryName.empty() ) )
   {
      LogContext(logContext).log(3, "Sanity check failed: parentEntryID: '"
         + parentEntryID + "'" + "entryName: '" + entryName + "'");

      // something entirely wrong, fail immediately, error already set above
      goto send_response;
   }


   /* Note: Actually we should first do a lookup. However, a successful create also implies a
             failed Lookup, so we can take a shortcut. */

   // create
   if (createFlag)
   {
      // checks in create() if lookup already found the entry
      createRes = create(this->getParentInfo(), entryName, &diskEntryInfo,
         &inodeData);

      FhgfsOpsErr sendCreateRes;
      if (createRes == FhgfsOpsErr_SUCCESS)
      {
         sendCreateRes = FhgfsOpsErr_SUCCESS;

         // Successful Create, which implies Lookup-before-create would have been failed.
         respMsg.setLookupResult(FhgfsOpsErr_PATHNOTEXISTS);

         respMsg.setEntryInfo(&diskEntryInfo);
      }
      else
      {
         if (createRes == FhgfsOpsErr_EXISTS)
         {
            // NOTE: we need to do a Lookup to get required lookup data

            if (getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATEEXCLUSIVE)
               sendCreateRes = FhgfsOpsErr_EXISTS;
            else
               sendCreateRes = FhgfsOpsErr_SUCCESS;

         }
         else
            sendCreateRes = FhgfsOpsErr_INTERNAL;
      }

      respMsg.addResponseCreate(sendCreateRes);

      // note: don't quit here on error because caller might still have requested stat info
   }

   // lookup
   if ((!createFlag) || (createRes == FhgfsOpsErr_EXISTS) )
   {
      lookupRes = lookup(parentEntryID, entryName, &diskEntryInfo, &inodeData);

      respMsg.setLookupResult(lookupRes);

      if (lookupRes == FhgfsOpsErr_SUCCESS)
         respMsg.setEntryInfo(&diskEntryInfo);

      if( (lookupRes != FhgfsOpsErr_SUCCESS) &&
         !(getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE) )
         goto send_response; // stop if lookup failed and we don't want to create a new file
   }

   // revalidate

   if(getIntentFlags() & LOOKUPINTENTMSG_FLAG_REVALIDATE)
   {
      FhgfsOpsErr revalidateRes = revalidate(&diskEntryInfo);

      respMsg.addResponseRevalidate(revalidateRes);

      if(revalidateRes != FhgfsOpsErr_SUCCESS)
         goto send_response;
   }


   // stat
   /* note: we do stat before open to avoid the dyn attribs refresh if the file is not opened by
      someone else currently. */

   if(getIntentFlags() & LOOKUPINTENTMSG_FLAG_STAT)
   {
      // check if lookup and create failed (we don't have an entryID to stat then)
      if(diskEntryInfo.getEntryID().empty() )
         goto send_response;

      bool loadFromDisk;
      if (inodeData.getDentryFeatureFlags() & DISKMETADATA_FEATURE_INODE_INLINE)
         loadFromDisk = false;
      else
         loadFromDisk = true;

      /* Note: We always call stat() here. With loadFromDisk == false, it will only return
       *       statData if the corresponding inode was referenced in memory.  */
      StatData statData;
      FhgfsOpsErr statRes = stat(&diskEntryInfo, loadFromDisk, statData);

      if (statRes != FhgfsOpsErr_SUCCESS && loadFromDisk == false)
      {
         // inode not loaded, we can take the data from the dentry

         StatData* dentryStatData = inodeData.getInodeStatData();

         respMsg.addResponseStat(FhgfsOpsErr_SUCCESS, dentryStatData);
      }
      else
      { // inode is not inlined, but has to be read separately

         respMsg.addResponseStat(statRes, &statData);

         if(statRes != FhgfsOpsErr_SUCCESS)
            goto send_response;
      }
   }


   // open

   if(getIntentFlags() & LOOKUPINTENTMSG_FLAG_OPEN)
   {
      StripePattern* pattern = NULL;

      // don't open if create failed
      if ((createRes != FhgfsOpsErr_SUCCESS) && (getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE) )
         goto send_response;

      // check if lookup and/or create failed (we don't have an entryID to open then)
      if(diskEntryInfo.getEntryID().empty() )
         goto send_response;


      FhgfsOpsErr openRes = open(&diskEntryInfo, &fileHandleID, &pattern);

      if(openRes != FhgfsOpsErr_SUCCESS)
      { // open failed => use dummy pattern for response
         respMsg.addResponseOpen(openRes, fileHandleID, &dummyPattern);

         goto send_response;
      }

      respMsg.addResponseOpen(openRes, fileHandleID, pattern);
   }


send_response:

   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );


   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), this->getOpCounterType() );

   return true;
}

FhgfsOpsErr LookupIntentMsgEx::lookup(std::string& parentEntryID, std::string& entryName,
   EntryInfo* outEntryInfo, DentryInodeMeta* outInodeMetaData)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   DirInode* parentDir = metaStore->referenceDir(parentEntryID, false);
   if(!parentDir)
   { // out of memory
      return FhgfsOpsErr_INTERNAL;
   }

   FhgfsOpsErr lookupRes;

   DirEntryType subEntryType = parentDir->getEntryInfo(entryName, outEntryInfo, outInodeMetaData);

   /* FIXME Bernd: If the entry has invalid meta-data, we return FhgfsOpsErr_PATHNOTEXISTS
    *              This also makes it impossible to delete the entry, as the client assumes
    *              the entry already does not exist anymore.
    */
   if(DirEntryType_ISDIR(subEntryType) || DirEntryType_ISFILE(subEntryType) )
   { // next entry exists and is a file
      lookupRes = FhgfsOpsErr_SUCCESS;
   }
   else
   { // entry does not exist
      lookupRes = FhgfsOpsErr_PATHNOTEXISTS;
   }

   metaStore->releaseDir(parentEntryID);

   return lookupRes;
}

/**
 * compare entryInfo on disk with EntryInfo send by the client
 * @return FhgfsOpsErr_SUCCESS if revalidation successful, FhgfsOpsErr_PATHNOTEXISTS otherwise.
 */
FhgfsOpsErr LookupIntentMsgEx::revalidate(EntryInfo* diskEntryInfo)
{
   EntryInfo* clientEntryInfo = this->getEntryInfo();

   if ( (diskEntryInfo->getEntryID()     == clientEntryInfo->getEntryID() ) &&
        (diskEntryInfo->getOwnerNodeID() == clientEntryInfo->getOwnerNodeID() ) )
      return FhgfsOpsErr_SUCCESS;


   return FhgfsOpsErr_PATHNOTEXISTS;
}

FhgfsOpsErr LookupIntentMsgEx::create(EntryInfo* parentInfo,
   std::string& entryName, EntryInfo *outEntryInfo, DentryInodeMeta* outInodeData)
{
   MkFileDetails mkDetails(entryName, getUserID(), getGroupID(), getMode() );

   UInt16List preferredTargets;
   parsePreferredTargets(&preferredTargets);

   return MsgHelperMkFile::mkFile(parentInfo, &mkDetails, &preferredTargets,
      outEntryInfo, outInodeData);
}

FhgfsOpsErr LookupIntentMsgEx::stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData)
{
   Node* localNode = Program::getApp()->getLocalNode();

   FhgfsOpsErr statRes = FhgfsOpsErr_NOTOWNER;

   // check if we can stat on this machine or if entry is owned by another server
   if(entryInfo->getOwnerNodeID() == localNode->getNumID() )
      statRes = MsgHelperStat::stat(entryInfo, loadFromDisk, outStatData);

   return statRes;
}

/**
 * @param outPattern only set if success is returned; points to referenced open file, so it does
 * not need to be free'd/deleted.
 */
FhgfsOpsErr LookupIntentMsgEx::open(EntryInfo* entryInfo, std::string* outFileHandleID,
   StripePattern** outPattern)
{
   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();

   FileInode* openFile;

   FhgfsOpsErr openRes = MsgHelperOpen::openFile(entryInfo, getAccessFlags(), &openFile);

   if(openRes != FhgfsOpsErr_SUCCESS)
      return openRes; // error occurred


   // open successful => insert session

   SessionFile* sessionFile = new SessionFile(openFile, getAccessFlags(), entryInfo);

   Session* session = sessions->referenceSession(getSessionID(), true);

   *outPattern = openFile->getStripePattern();

   unsigned ownerFD = session->getFiles()->addSession(sessionFile);

   sessions->releaseSession(session);

   *outFileHandleID = SessionTk::generateFileHandleID(ownerFD, entryInfo->getEntryID() );

   return openRes;
}

/**
 * Decide which type of client stats op counter we increase for this msg (based on given msg flags).
 */
MetaOpCounterTypes LookupIntentMsgEx::getOpCounterType()
{
   /* note: as introducting a speparate opCounter type for each flag would have been too much,
      we assign prioritiess here as follows: create > open > revalidate > stat > simple_no_flags */

   /* NOTE: Those if's are rather slow, maybe we should create a table that has the values?
    *       Problem is that the table has to be filled for flag combinations, which is also ugly
    */

   if(this->getIntentFlags() & LOOKUPINTENTMSG_FLAG_CREATE)
      return MetaOpCounter_LOOKUPINTENT_CREATE;

   if(this->getIntentFlags() & LOOKUPINTENTMSG_FLAG_OPEN)
      return MetaOpCounter_LOOKUPINTENT_OPEN;

   if(this->getIntentFlags() & LOOKUPINTENTMSG_FLAG_REVALIDATE)
      return MetaOpCounter_LOOKUPINTENT_REVALIDATE;

   if(this->getIntentFlags() & LOOKUPINTENTMSG_FLAG_STAT)
      return MetaOpCounter_LOOKUPINTENT_STAT;

   return MetaOpCounter_LOOKUPINTENT_SIMPLE;

}
