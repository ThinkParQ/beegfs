#include "BuddyResyncerModSyncSlave.h"

#include <common/net/message/storage/mirroring/ResyncRawInodesRespMsg.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/DebugVariable.h>
#include <common/Common.h>

#include <net/message/storage/mirroring/ResyncRawInodesMsgEx.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <program/Program.h>
#include <toolkit/XAttrTk.h>

BuddyResyncerModSyncSlave::BuddyResyncerModSyncSlave(BuddyResyncJob& parentJob,
      MetaSyncCandidateStore* syncCandidates, uint8_t slaveID, const NumNodeID& buddyNodeID) :
   SyncSlaveBase("BuddyResyncerModSyncSlave_" + StringTk::uintToStr(slaveID), parentJob,
         buddyNodeID),
   syncCandidates(syncCandidates)
{
}

void BuddyResyncerModSyncSlave::syncLoop()
{
   while (!getSelfTerminateNotIdle())
   {
      if (syncCandidates->waitForFiles(this))
         resyncAt(Path(), false, streamCandidates, this);
      else if (getOnlyTerminateIfIdle())
         break;
   }
}

namespace {
struct CandidateSignaler
{
   void operator()(MetaSyncCandidateFile* candidate) const
   {
      candidate->signal();
   }
};

bool resyncElemCmp(const MetaSyncCandidateFile::Element& a, const MetaSyncCandidateFile::Element& b)
{
   // we must sync deletions before updates and inodes before everything else:
   //
   // deletions may fail on the secondary, so they *can* be synced first to begin with.
   // any item that is deleted and then recreated with an update must be deleted first.
   // we also guarantee that no item is created and deleted in the same changeset.
   //
   // inodes must be synced before dentries because the dentries may link to inodes in the same
   // changeset - and if the secondary does not have the appropriate inode yet, the changeset
   // must create it.
   if (a.isDeletion && !b.isDeletion)
      return true;

   if (a.type == MetaSyncFileType::Inode && b.type != MetaSyncFileType::Inode)
      return true;

   return std::make_pair(int(a.type), a.path) < std::make_pair(int(b.type), b.path);
}
}

FhgfsOpsErr BuddyResyncerModSyncSlave::streamCandidates(Socket& socket)
{
   DEBUG_ENV_VAR(unsigned, DEBUG_FAIL_MODSYNC, 0, "BEEGFS_DEBUG_FAIL_MODSYNC");

   while (!getSelfTerminateNotIdle())
   {
      if (syncCandidates->isFilesEmpty())
         break;

      MetaSyncCandidateFile candidate;
      syncCandidates->fetch(candidate, this);

      // signal the candidate at the end of this loop iteration.
      // do it like this because we have a few exit points and also have exceptions to take into
      // account.
      std::unique_ptr<MetaSyncCandidateFile, CandidateSignaler> signaler(&candidate);

      auto resyncElems = candidate.releaseElements();

      std::sort(resyncElems.begin(), resyncElems.end(), resyncElemCmp);

      for (auto it = resyncElems.begin(); it != resyncElems.end(); ++it)
      {
         const auto& element = *it;

         // element.path is relative to the meta root, so we have to chop off the buddymir/ prefix
         const Path itemPath(element.path.substr(strlen(META_BUDDYMIRROR_SUBDIR_NAME) + 1));

         FhgfsOpsErr resyncRes;

         LOG_DBG(MIRRORING, DEBUG, "Syncing one modification.", element.path, element.isDeletion,
               int(element.type));

         switch (element.type)
         {
            case MetaSyncFileType::Dentry:
               resyncRes = element.isDeletion
                  ? deleteDentry(socket, itemPath.dirname(), itemPath.back())
                  : streamDentry(socket, itemPath.dirname(), itemPath.back());
               break;

            case MetaSyncFileType::Directory:
            case MetaSyncFileType::Inode:
               resyncRes = element.isDeletion
                  ? deleteInode(socket, itemPath, element.type == MetaSyncFileType::Directory)
                  : streamInode(socket, itemPath, element.type == MetaSyncFileType::Directory);
               break;

            default:
               LOG(MIRRORING, ERR, "this should never happen");
               return FhgfsOpsErr_INTERNAL;
         }

         if (resyncRes != FhgfsOpsErr_SUCCESS || DEBUG_FAIL_MODSYNC)
         {
            LOG(MIRRORING, ERR, "Modification resync failed.", element.path, element.isDeletion,
                resyncRes);
            numErrors.increase();

            // Since this error prevents the resync from reaching a GOOD state on the secondary,
            // we abort here.
            parentJob->abort(true);

            // terminate the current stream, start a new one if necessary. we could (in theory)
            // reuse the current stream, but terminating a stream that has seen an error is simpler
            // to handle than keeping it open. also, bulk resync would like "fail on error"
            // semantics very much.
            sendResyncPacket(socket, std::tuple<>());
            return FhgfsOpsErr_SUCCESS;
         }
         else
         {
            numObjectsSynced.increase();
         }
      }
   }

   sendResyncPacket(socket, std::tuple<>());
   return FhgfsOpsErr_SUCCESS;
}
