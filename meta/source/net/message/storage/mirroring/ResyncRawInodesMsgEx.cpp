#include "ResyncRawInodesMsgEx.h"

#include <common/app/log/Logger.h>
#include <common/net/message/storage/mirroring/ResyncRawInodesRespMsg.h>
#include <app/App.h>
#include <components/buddyresyncer/SyncCandidate.h>
#include <net/message/storage/mirroring/SetMetadataMirroringMsgEx.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <program/Program.h>
#include <toolkit/XAttrTk.h>

#include <dirent.h>

bool ResyncRawInodesMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(MIRRORING, DEBUG, "", basePath, hasXAttrs, wholeDirectory);

   const FhgfsOpsErr resyncRes = resyncStream(ctx);

   ctx.sendResponse(ResyncRawInodesRespMsg(resyncRes));
   return resyncRes == FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr ResyncRawInodesMsgEx::resyncStream(ResponseContext& ctx)
{
   if (hasXAttrs && !Program::getApp()->getConfig()->getStoreClientXAttrs())
   {
      LOG(MIRRORING, ERR, "Primary has indicated xattr resync, but xattrs are disabled in config.");
      return FhgfsOpsErr_NOTSUPP;
   }

   const auto& rootInfo = Program::getApp()->getMetaRoot();
   auto* const metaBGM = Program::getApp()->getMetaBuddyGroupMapper();
   auto* const rootDir = Program::getApp()->getRootDir();

   // if the local root is not buddyMirrored yet, set local buddy mirroring for the root inode.
   if (rootInfo.getID().val() == metaBGM->getLocalGroupID() &&
         !rootDir->getIsBuddyMirrored())
   {
      const auto setMirrorRes = SetMetadataMirroringMsgEx::setMirroring();
      if (setMirrorRes != FhgfsOpsErr_SUCCESS)
      {
         LOG(MIRRORING, ERR, "Failed to set meta mirroring on the root directory", setMirrorRes);
         return setMirrorRes;
      }
   }

   // if our path is a directory, we must create it now, otherwise, the directory may not be
   // created at all. for example the #fSiDs# directory in an empty content directory with no
   // orphaned fsids would not be created.
   if (wholeDirectory)
   {
      const auto mkRes = Program::getApp()->getMetaStore()->beginResyncFor(
            META_BUDDYMIRROR_SUBDIR_NAME / basePath, true);
      if (mkRes.first != FhgfsOpsErr_SUCCESS)
      {
         LOG(MIRRORING, ERR, "Failed to create metadata directory.", basePath,
             ("mkRes", mkRes.first));
         return mkRes.first;
      }
   }

   while (true)
   {
      const auto resyncPartRes = resyncSingle(ctx);
      if (resyncPartRes == FhgfsOpsErr_AGAIN)
         continue;
      else if (resyncPartRes == FhgfsOpsErr_SUCCESS)
         break;

      return resyncPartRes;
   }

   const FhgfsOpsErr result = wholeDirectory
      ? removeUntouchedInodes()
      : FhgfsOpsErr_SUCCESS;

   return result;
}

FhgfsOpsErr ResyncRawInodesMsgEx::resyncSingle(ResponseContext& ctx)
{
   uint32_t packetLength;

   ctx.getSocket()->recvExact(&packetLength, sizeof(packetLength), 0);
   packetLength = LE_TO_HOST_32(packetLength);

   if (packetLength == 0)
      return FhgfsOpsErr_SUCCESS;

   std::unique_ptr<char[]> packetData(new char[packetLength]);

   ctx.getSocket()->recvExact(packetData.get(), packetLength, 0);

   Deserializer des(packetData.get(), packetLength);

   MetaSyncFileType packetType;
   std::string relPath;

   des
      % packetType
      % relPath;
   if (!des.good())
   {
      LOG(MIRRORING, ERR, "Received bad data from primary.");
      return FhgfsOpsErr_INTERNAL;
   }

   if (wholeDirectory)
      inodesWritten.push_back(relPath);

   FhgfsOpsErr result;

   switch (packetType)
   {
      case MetaSyncFileType::Inode:
      case MetaSyncFileType::Directory:
         result = resyncInode(ctx, basePath / relPath, des,
               packetType == MetaSyncFileType::Directory);
         break;

      case MetaSyncFileType::Dentry:
         result = resyncDentry(ctx, basePath / relPath, des);
         break;

      default:
         result = FhgfsOpsErr_INVAL;
   }

   ctx.sendResponse(ResyncRawInodesRespMsg(result));

   // if the resync has failed, we have to return the result twice - once as an ACK for the packet,
   // and another time to terminate the stream. mod sync could do without the termination, but
   // bulk resync can't.
   if (result == FhgfsOpsErr_SUCCESS)
      return FhgfsOpsErr_AGAIN;
   else
      return result;
}

FhgfsOpsErr ResyncRawInodesMsgEx::resyncInode(ResponseContext& ctx, const Path& path,
   Deserializer& data, const bool isDirectory, const bool recvXAttrs)
{
   std::vector<char> content;
   bool isDeletion;

   data
      % content
      % isDeletion;
   if (!data.good())
   {
      LOG(MIRRORING, ERR, "Received bad data from primary.");
      return FhgfsOpsErr_INTERNAL;
   }

   if (isDeletion)
   {
      const bool rmRes = isDirectory
         ? StorageTk::removeDirRecursive((META_BUDDYMIRROR_SUBDIR_NAME / path).str())
         : unlink((META_BUDDYMIRROR_SUBDIR_NAME / path).str().c_str()) == 0;

      if (rmRes || errno == ENOENT)
         return FhgfsOpsErr_SUCCESS;

      LOG(MIRRORING, ERR, "Failed to remove raw meta inode.", path, sysErr);
      return FhgfsOpsErr_INTERNAL;
   }

   if (!isDirectory && wholeDirectory)
   {
      const auto unlinkRes = Program::getApp()->getMetaStore()->unlinkRawMetadata(
            META_BUDDYMIRROR_SUBDIR_NAME / path);
      if (unlinkRes != FhgfsOpsErr_SUCCESS && unlinkRes != FhgfsOpsErr_PATHNOTEXISTS)
      {
         LOG(MIRRORING, ERR, "Could not unlink raw metadata", path, unlinkRes);
         return FhgfsOpsErr_INTERNAL;
      }
   }

   auto inode = Program::getApp()->getMetaStore()->beginResyncFor(
         META_BUDDYMIRROR_SUBDIR_NAME / path, isDirectory);
   if (inode.first)
      return inode.first;

   if (!isDirectory)
   {
      const auto setContentRes = inode.second.setContent(&content[0], content.size());
      if (setContentRes)
         return setContentRes;
   }

   if (!hasXAttrs || !recvXAttrs)
      return FhgfsOpsErr_SUCCESS;

   const auto xattrRes = resyncInodeXAttrs(ctx, inode.second);
   if (xattrRes != FhgfsOpsErr_SUCCESS)
   {
      LOG(MIRRORING, ERR, "Syncing XAttrs failed.", path, xattrRes);
      return xattrRes;
   }

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr ResyncRawInodesMsgEx::resyncDentry(ResponseContext& ctx, const Path& path,
   Deserializer& data)
{
   bool linksToFsID;

   data % linksToFsID;
   if (!data.good())
   {
      LOG(MIRRORING, ERR, "Received bad data from primary.");
      return FhgfsOpsErr_INTERNAL;
   }

   // dentries with independent contents (dir dentries, dentries to non-inlined files) can be
   // treated like inodes for the purpose of resync. don't sync xattrs though, because dentries
   // should never have them
   if (!linksToFsID)
      return resyncInode(ctx, path, data, false, false);

   std::string targetID;
   bool isDeletion;

   data
      % targetID
      % isDeletion;
   if (!data.good())
   {
      LOG(MIRRORING, ERR, "Received bad data from primary.");
      return FhgfsOpsErr_INTERNAL;
   }

   const FhgfsOpsErr rmRes = Program::getApp()->getMetaStore()->unlinkRawMetadata(
         META_BUDDYMIRROR_SUBDIR_NAME / path);
   if (rmRes != FhgfsOpsErr_SUCCESS && rmRes != FhgfsOpsErr_PATHNOTEXISTS)
   {
      LOG(MIRRORING, ERR, "Could not unlink old dentry.", path, rmRes);
      return FhgfsOpsErr_INTERNAL;
   }

   if (isDeletion)
      return FhgfsOpsErr_SUCCESS;

   const Path& idPath = path.dirname() / META_DIRENTRYID_SUB_STR / targetID;
   const int linkRes = ::link(
         (META_BUDDYMIRROR_SUBDIR_NAME / idPath).str().c_str(),
         (META_BUDDYMIRROR_SUBDIR_NAME / path).str().c_str());
   if (linkRes < 0)
   {
      LOG(MIRRORING, ERR, "Could not link dentry to fsid.", path, idPath, sysErr);
      return FhgfsOpsErr_INTERNAL;
   }

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr ResyncRawInodesMsgEx::resyncInodeXAttrs(ResponseContext& ctx, IncompleteInode& inode)
{
   std::string name;
   std::vector<char> value;

   while (true)
   {
      auto readRes = MsgHelperXAttr::StreamXAttrState::readNextXAttr(ctx.getSocket(), name, value);
      if (readRes == FhgfsOpsErr_SUCCESS)
         break;
      else if (readRes != FhgfsOpsErr_AGAIN)
         return readRes;

      auto setRes = inode.setXattr((XAttrTk::UserXAttrPrefix + name).c_str(), &value[0],
            value.size());
      if (setRes != FhgfsOpsErr_SUCCESS)
         return setRes;
   }

   return inode.clearUnsetXAttrs();
}

FhgfsOpsErr ResyncRawInodesMsgEx::removeUntouchedInodes()
{
   std::sort(inodesWritten.begin(), inodesWritten.end());

   const Path dirPath(META_BUDDYMIRROR_SUBDIR_NAME / basePath);

   std::unique_ptr<DIR, StorageTk::CloseDirDeleter> dir(::opendir(dirPath.str().c_str()));

   if (!dir)
   {
      LOG(MIRRORING, ERR, "Could not open meta directory.", dirPath, sysErr);
      return FhgfsOpsErr_INTERNAL;
   }

   int dirFD = ::dirfd(dir.get());
   if (dirFD < 0)
   {
      LOG(MIRRORING, ERR, "Could not get directory fd.", sysErr);
      return FhgfsOpsErr_INTERNAL;
   }

   while (true)
   {
      struct dirent* found;

#if USE_READDIR_P
      struct dirent entry;
      int err = readdir_r(dir.get(), &entry, &found);
#else
      errno = 0;
      found = readdir(dir.get());
      int err = found ? 0 : errno;
#endif
      if (err > 0)
      {
         LOG(MIRRORING, ERR, "readdir() failed.", sysErr(err));
         return FhgfsOpsErr_INTERNAL;
      }

      if (!found)
         break;

      if (strcmp(found->d_name, ".") == 0 || strcmp(found->d_name, "..") == 0)
         continue;

      bool written = std::binary_search(
            inodesWritten.begin(), inodesWritten.end(),
            found->d_name);
      if (written)
         continue;

      const int unlinkRes = ::unlinkat(dirFD, found->d_name, 0);
      if (unlinkRes == 0 || errno == ENOENT)
         continue;

      if (errno != EISDIR)
      {
         LOG(MIRRORING, ERR, "Could not remove file", basePath, found->d_name, sysErr);
         return FhgfsOpsErr_INTERNAL;
      }

      const bool rmRes = StorageTk::removeDirRecursive((dirPath / found->d_name).str());
      if (!rmRes)
      {
         LOG(MIRRORING, ERR, "Could not remove file", found->d_name, sysErr);
         return FhgfsOpsErr_INTERNAL;
      }
   }

   return FhgfsOpsErr_SUCCESS;
}
