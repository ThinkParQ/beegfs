#include "SyncSlaveBase.h"

#include <common/net/message/storage/mirroring/ResyncRawInodesRespMsg.h>
#include <net/message/storage/mirroring/ResyncRawInodesMsgEx.h>
#include <net/msghelpers/MsgHelperXAttr.h>
#include <program/Program.h>
#include <toolkit/XAttrTk.h>

void SyncSlaveBase::run()
{
   setIsRunning(true);

   try
   {
      LOG(MIRRORING, DEBUG, "Component started");

      registerSignalHandler();

      syncLoop();

      LOG(MIRRORING, DEBUG, "Component stopped");
   }
   catch (std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

   setIsRunning(false);
}

FhgfsOpsErr SyncSlaveBase::receiveAck(Socket& socket)
{
   auto resp = MessagingTk::recvMsgBuf(socket);
   if (resp.empty())
      return FhgfsOpsErr_INTERNAL;

   const auto respMsg = PThread::getCurrentThreadApp()->getNetMessageFactory()->createFromBuf(
         std::move(resp));
   if (respMsg->getMsgType() != NETMSGTYPE_ResyncRawInodesResp)
      return FhgfsOpsErr_COMMUNICATION;

   return static_cast<ResyncRawInodesRespMsg&>(*respMsg).getResult();
}

FhgfsOpsErr SyncSlaveBase::resyncAt(const Path& basePath, bool wholeDirectory,
   FhgfsOpsErr (*streamFn)(Socket*, void*), void* context)
{
   const bool sendXAttrs = Program::getApp()->getConfig()->getStoreClientXAttrs();

   this->basePath = META_BUDDYMIRROR_SUBDIR_NAME / basePath;

   ResyncRawInodesMsgEx msg(basePath, sendXAttrs, wholeDirectory);

   RequestResponseNode rrNode(buddyNodeID, Program::getApp()->getMetaNodes());
   RequestResponseArgs rrArgs(nullptr, &msg, NETMSGTYPE_ResyncRawInodesResp,
         streamFn, context);

   // resync processing may take a very long time for each step, eg if a very large directory must
   // be cleaned out on the secondary. do not use timeouts for resync communication right now.

   rrArgs.minTimeoutMS = -1;

   const auto commRes = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

   if (commRes != FhgfsOpsErr_SUCCESS)
   {
      LOG(MIRRORING, ERR, "Error during communication with secondary.", commRes);
      return commRes;
   }

   const auto resyncRes = static_cast<ResyncRawInodesRespMsg&>(*rrArgs.outRespMsg).getResult();

   if (resyncRes != FhgfsOpsErr_SUCCESS)
      LOG(MIRRORING, ERR, "Error while resyncing directory.", basePath, resyncRes);

   return resyncRes;
}

FhgfsOpsErr SyncSlaveBase::streamDentry(Socket& socket, const Path& contDirRelPath,
   const std::string& name)
{
   std::unique_ptr<DirEntry> dentry(
         DirEntry::createFromFile((basePath / contDirRelPath).str(), name));
   if (!dentry)
   {
      LOG(MIRRORING, ERR, "Could not open dentry.", basePath, contDirRelPath, name);
      return FhgfsOpsErr_INTERNAL;
   }

   if (dentry->getIsInodeInlined())
   {
      auto err = sendResyncPacket(socket, LinkDentryInfo(
            MetaSyncFileType::Dentry,
            (contDirRelPath / name).str(),
            true,
            dentry->getID(),
            false));
      if (err != FhgfsOpsErr_SUCCESS)
         return err;

      return receiveAck(socket);
   }

   std::vector<char> dentryContent;

   {
      Serializer ser;
      dentry->serializeDentry(ser);
      dentryContent.resize(ser.size());

      ser = Serializer(&dentryContent[0], dentryContent.size());
      dentry->serializeDentry(ser);
      if (!ser.good())
      {
         LOG(MIRRORING, ERR, "Could not serialize dentry for secondary.");
         return FhgfsOpsErr_INTERNAL;
      }
   }

   const FhgfsOpsErr sendRes = sendResyncPacket(socket, FullDentryInfo(
         MetaSyncFileType::Dentry,
         (contDirRelPath / name).str(),
         false,
         dentryContent,
         false));
   if (sendRes != FhgfsOpsErr_SUCCESS)
      return sendRes;

   return receiveAck(socket);
}

FhgfsOpsErr SyncSlaveBase::streamInode(Socket& socket, const Path& inodeRelPath,
   const bool isDirectory)
{
   const Path fullPath(basePath / inodeRelPath);

   MetaStore& store = *Program::getApp()->getMetaStore();

   std::vector<char> contents;

   if (!isDirectory)
   {
      const FhgfsOpsErr readRes = store.getRawMetadata(fullPath, contents);
      if (readRes != FhgfsOpsErr_SUCCESS)
         return readRes;
   }

   const FhgfsOpsErr sendRes = sendResyncPacket(socket, InodeInfo(
         isDirectory
            ? MetaSyncFileType::Directory
            : MetaSyncFileType::Inode,
         inodeRelPath.str(),
         contents,
         false));
   if (sendRes != FhgfsOpsErr_SUCCESS)
      return sendRes;

   if (Program::getApp()->getConfig()->getStoreClientXAttrs())
   {
      auto xattrs = XAttrTk::listUserXAttrs(fullPath.str());
      if (xattrs.first != FhgfsOpsErr_SUCCESS)
      {
         LOG(MIRRORING, ERR, "Could not list resync candidate xattrs.", fullPath, ("error", xattrs.first));
         xattrs.second.clear();
         return FhgfsOpsErr_INTERNAL;
      }

      MsgHelperXAttr::StreamXAttrState state(fullPath.str(), std::move(xattrs.second));

      const FhgfsOpsErr xattrRes = MsgHelperXAttr::StreamXAttrState::streamXattrFn(&socket, &state);
      if (xattrRes != FhgfsOpsErr_SUCCESS)
      {
         LOG(MIRRORING, ERR, "Error while sending xattrs to secondary.", fullPath, xattrRes);
         return FhgfsOpsErr_INTERNAL;
      }
   }

   return receiveAck(socket);
}

FhgfsOpsErr SyncSlaveBase::deleteDentry(Socket& socket, const Path& contDirRelPath,
   const std::string& name)
{
   auto err = sendResyncPacket(socket, LinkDentryInfo(
         MetaSyncFileType::Dentry,
         (contDirRelPath / name).str(),
         true,
         {},
         true));
   if (err != FhgfsOpsErr_SUCCESS)
      return err;

   return receiveAck(socket);
}

FhgfsOpsErr SyncSlaveBase::deleteInode(Socket& socket, const Path& inodeRelPath,
   const bool isDirectory)
{
   auto err = sendResyncPacket(socket, InodeInfo(
         isDirectory
            ? MetaSyncFileType::Directory
            : MetaSyncFileType::Inode,
         inodeRelPath.str(),
         {},
         true));
   if (err != FhgfsOpsErr_SUCCESS)
      return err;

   return receiveAck(socket);
}
