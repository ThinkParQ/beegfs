#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "MsgHelperMkFile.h"

/**
 * @param dir current directory
 * @param currentDepth 1-based path depth
 */
FhgfsOpsErr MsgHelperMkFile::mkFile(EntryInfo* parentInfo, MkFileDetails* mkDetails,
   UInt16List* preferredTargets, EntryInfo* outEntryInfo, DentryInodeMeta* outInodeData)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   // reference parent
   DirInode* dir = metaStore->referenceDir(parentInfo->getEntryID(), true);
   if(!dir)
      return FhgfsOpsErr_PATHNOTEXISTS;

   // create meta file
   retVal = metaStore->mkMetaFile(dir, mkDetails, preferredTargets, NULL, outEntryInfo,
      outInodeData);

   // clean-up
   metaStore->releaseDir(parentInfo->getEntryID() );

   return retVal;
}

