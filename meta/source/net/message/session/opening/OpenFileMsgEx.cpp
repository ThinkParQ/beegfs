#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <common/toolkit/SessionTk.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperOpen.h>
#include <program/Program.h>
#include <session/EntryLock.h>
#include <session/SessionStore.h>
#include "OpenFileMsgEx.h"

FileIDLock OpenFileMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

bool OpenFileMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "OpenFileMsg incoming";

   EntryInfo* entryInfo = getEntryInfo();

   LOG_DEBUG(logContext, Log_SPAM, "ParentInfo: " + entryInfo->getParentEntryID() +
      " EntryID: " + entryInfo->getEntryID() + " FileName: " + entryInfo->getFileName() );

   LOG_DEBUG(logContext, Log_DEBUG,
      "BuddyMirrored: " + std::string(entryInfo->getIsBuddyMirrored() ? "Yes" : "No") +
      " Secondary: " +
      std::string(hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) ? "Yes" : "No") );
#endif // BEEGFS_DEBUG

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> OpenFileMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   App* app = Program::getApp();

   EntryInfo* entryInfo = this->getEntryInfo();
   bool useQuota = isMsgHeaderFeatureFlagSet(OPENFILEMSG_FLAG_USE_QUOTA);
   bool bypassAccessCheck = isMsgHeaderFeatureFlagSet(OPENFILEMSG_FLAG_BYPASS_ACCESS_CHECK);

   const bool eventLoggingEnabled = !isSecondary && app->getFileEventLogger() && getFileEvent();
   MetaFileHandle inode;
   FileState fileState;
   bool haveFileState = false;

   PathInfo pathInfo;
   StripePattern* pattern;
   unsigned sessionFileID;
   SessionStore* sessions = entryInfo->getIsBuddyMirrored()
      ? app->getMirroredSessions()
      : app->getSessions();

   FhgfsOpsErr openRes = MsgHelperOpen::openFile(
      entryInfo, getAccessFlags(), useQuota, bypassAccessCheck, getMsgHeaderUserID(),
      inode, isSecondary);

   if (openRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(*inode, fileTimestamps, entryInfo);

   // update operation counters
   updateNodeOp(ctx, MetaOpCounter_OPEN);

   if (openRes != FhgfsOpsErr_SUCCESS)
   {
      // Handle file access denied - get file state and optionally log event
      if (openRes == FhgfsOpsErr_FILEACCESS_DENIED)
      {
         // Reference inode to read file state. No isSecondary guard needed here
         // because FILEACCESS_DENIED is never forwarded to secondary
         // (changesObservableState() returns true only on SUCCESS).
         auto [refInode, refRes] = app->getMetaStore()->referenceFile(entryInfo);
         if (refInode)
         {
            fileState = refInode->getFileState();
            haveFileState = true;
            app->getMetaStore()->releaseFile(entryInfo->getParentEntryID(), refInode);
         }

         // Emit OPEN_BLOCKED event if logging enabled
         if (eventLoggingEnabled)
         {
            FileEvent* fileEvent = const_cast<FileEvent*>(getFileEvent());
            fileEvent->type = FileEventType::OPEN_BLOCKED;

            EventContext eventCtx = makeEventContext(
               entryInfo,
               entryInfo->getParentEntryID(),
               getMsgHeaderUserID(),
               "",
               inode ? inode->getNumHardlinks() : 0,
               isSecondary
            );
            logEvent(app->getFileEventLogger(), *fileEvent, eventCtx);
         }
      }

      // generate response
      if(unlikely(openRes == FhgfsOpsErr_COMMUNICATION))
      {
         return boost::make_unique<OpenFileResponseState>();
      }
      else
      { // normal response
         Raid0Pattern dummyPattern(1, UInt16Vector{});
         return boost::make_unique<OpenFileResponseState>(openRes, std::string(), dummyPattern,
               pathInfo, 0, fileState, haveFileState);
      }
   }

   if (eventLoggingEnabled)
   {
      EventContext eventCtx = makeEventContext(
         entryInfo,
         entryInfo->getParentEntryID(),
         getMsgHeaderUserID(),
         "",
         inode->getNumHardlinks(),
         isSecondary
      );

      logEvent(app->getFileEventLogger(), *getFileEvent(), eventCtx);
   }

   // success => insert session
   SessionFile* sessionFile = new SessionFile(std::move(inode), getAccessFlags(), entryInfo);

   Session* session = sessions->referenceSession(getClientNumID(), true);

   pattern = sessionFile->getInode()->getStripePattern();

   if (!isSecondary)
   {
      sessionFileID = session->getFiles()->addSession(sessionFile);
      fileHandleID = SessionTk::generateFileHandleID(sessionFileID, entryInfo->getEntryID() );

      setSessionFileID(sessionFileID);
      setFileHandleID(fileHandleID.c_str());
   }
   else
   {
      fileHandleID = getFileHandleID();
      sessionFileID = getSessionFileID();

      bool addRes = session->getFiles()->addSession(sessionFile, sessionFileID);

      if (!addRes)
      {
         const char* logContext = "OpenFileMsgEx (executeLocally)";
         LogContext(logContext).log(Log_NOTICE,
            "Couldn't add sessionFile on secondary buddy; sessionID: " + getClientNumID().str()
               + "; sessionFileID: " + StringTk::uintToStr(sessionFileID));
      }
   }

   sessions->releaseSession(session);

   sessionFile->getInode()->getPathInfo(&pathInfo);

   // On successful open(), fileState is not encoded in compat flags (haveFileState stays false)
   // because the client only needs it when open() is denied due to file state restrictions. We
   // don't pass a fileState here to avoid having to call sessionFile->getInode()->getFileState().
   return boost::make_unique<OpenFileResponseState>(openRes, fileHandleID, *pattern, pathInfo,
         sessionFile->getInode()->getFileVersion(), FileState{});
}

void OpenFileMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_OpenFileResp);
}
