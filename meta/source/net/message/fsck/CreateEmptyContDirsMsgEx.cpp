#include "CreateEmptyContDirsMsgEx.h"
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool CreateEmptyContDirsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "CreateEmptyContDirsMsg incoming";

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   StringList failedIDs;

   for (auto iter = items.begin(); iter != items.end(); iter++)
   {
      const std::string& dirID = std::get<0>(*iter);
      const bool isBuddyMirrored = std::get<1>(*iter);

      std::string contentsDirStr = MetaStorageTk::getMetaDirEntryPath(
            isBuddyMirrored
               ? app->getBuddyMirrorDentriesPath()->str()
               : app->getDentriesPath()->str(), dirID);

      // create contents directory
      int mkRes = mkdir(contentsDirStr.c_str(), 0755);

      if ( mkRes != 0 )
      { // error
         LOG(GENERAL, ERR, "Unable to create contents directory.", contentsDirStr, sysErr);
         failedIDs.push_back(dirID);
         continue;
      }

      // create the dirEntryID directory, which allows access to inlined inodes via dirID access
      std::string contentsDirIDStr = MetaStorageTk::getMetaDirEntryIDPath(contentsDirStr);

      int mkDirIDRes = mkdir(contentsDirIDStr.c_str(), 0755);

      if ( mkDirIDRes != 0 )
      { // error
         LOG(GENERAL, ERR, "Unable to create dirEntryID directory.", contentsDirIDStr, sysErr);
         failedIDs.push_back(dirID);
         continue;
      }

      FileIDLock lock;

      if (isBuddyMirrored)
         lock = {Program::getApp()->getMirroredSessions()->getEntryLockStore(), dirID, true};

      // update the dir attribs

      DirInode* dirInode = metaStore->referenceDir(dirID, isBuddyMirrored, true);

      if (!dirInode)
      {
         LOG(GENERAL, ERR, "Unable to reference directory.", dirID);
         failedIDs.push_back(dirID);
         continue;
      }

      FhgfsOpsErr refreshRes = dirInode->refreshMetaInfo();

      if (refreshRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).log(Log_NOTICE, "Unable to refresh contents directory metadata: "
            + contentsDirStr + ". " + "SysErr: " + System::getErrString());
      }

      metaStore->releaseDir(dirID);
   }

   ctx.sendResponse(CreateEmptyContDirsRespMsg(&failedIDs) );

   return true;

}
