#include <program/Program.h>
#include <storage/MetaStore.h>

#include "SessionFile.h"

bool SessionFile::operator==(const SessionFile& other) const
{
   return accessFlags == other.accessFlags
      && sessionID == other.sessionID
      && entryInfo == other.entryInfo
      && useAsyncCleanup == other.useAsyncCleanup;
}

bool SessionFile::relinkInode(MetaStore& store)
{
   auto openRes = store.openFile(&entryInfo, accessFlags, inode, true);

   if (openRes == FhgfsOpsErr_SUCCESS)
      return true;

   LOG(SESSIONS, ERR, "Could not relink session for inode.",
         ("inode", entryInfo.getParentEntryID() + "/" + entryInfo.getEntryID()));
   return false;
}
