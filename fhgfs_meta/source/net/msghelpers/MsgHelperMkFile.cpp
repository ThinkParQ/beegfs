#include <common/toolkit/MessagingTk.h>
#include <components/ModificationEventFlusher.h>
#include <program/Program.h>
#include "MsgHelperMkFile.h"

/*
 * @param stripePattern can be NULL, in which case a new pattern gets created; should only be set
 * if this is the secondary buddy of a mirror group
 */
FhgfsOpsErr MsgHelperMkFile::mkFile(DirInode& parentDir, MkFileDetails* mkDetails,
   const UInt16List* preferredTargets, const unsigned numtargets, const unsigned chunksize, 
   StripePattern* stripePattern, EntryInfo* outEntryInfo, FileInodeStoreData* outInodeData,
   StoragePoolId storagePoolId)
{
   const char* logContext = "MsgHelperMkFile (create file)";

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   ModificationEventFlusher* modEventFlusher = app->getModificationEventFlusher();
   bool modEventLoggingEnabled = modEventFlusher->isLoggingEnabled();

   FhgfsOpsErr retVal;

   // create new stripe pattern
   if ( !stripePattern )
      stripePattern = parentDir.createFileStripePattern(preferredTargets, numtargets, chunksize,
         storagePoolId);

   // check availability of stripe targets
   if(unlikely(!stripePattern ||  stripePattern->getStripeTargetIDs()->empty() ) )
   {
      LogContext(logContext).logErr(
         "Unable to create stripe pattern. No storage targets available? "
         "File: " + mkDetails->newName);

      SAFE_DELETE(stripePattern);
      return FhgfsOpsErr_INTERNAL;
   }

   // create meta file
   retVal = metaStore->mkNewMetaFile(parentDir, mkDetails,
         std::unique_ptr<StripePattern>(stripePattern), outEntryInfo, outInodeData);

   if ( (modEventLoggingEnabled ) && ( outEntryInfo ) )
   {
      std::string entryID = outEntryInfo->getEntryID();
      modEventFlusher->add(ModificationEvent_FILECREATED, entryID);
   }

   return retVal;
}


