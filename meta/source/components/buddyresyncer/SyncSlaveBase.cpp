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

   // Map to store attribute name and its data
   std::map<std::string, std::vector<char>> contents;

   if (!isDirectory)
   {
      std::vector<char> attrData;
      FhgfsOpsErr readRes;

      // Helper function to read and store attribute data in map
      auto readAndStoreMetaAttribute = [&](const std::string& attrName)
      {
         attrData.clear();
         readRes = store.getRawMetadata(fullPath, attrName.c_str(), attrData);
         if (readRes != FhgfsOpsErr_SUCCESS)
            return false;
         contents.insert(std::make_pair(attrName, std::move(attrData)));
         return true;
      };

      // Handle META_XATTR_NAME ("user.fhgfs") separately because it can be stored as either
      // file contents or an extended attribute, depending on the 'storeUseExtendedAttribs'
      // configuration setting in the meta config. In contrast, all other metadata-specific
      // attributes are strictly stored as extended attributes and do not have the option to
      // be stored as file contents.
      if (!readAndStoreMetaAttribute(META_XATTR_NAME))
         return readRes;

      // Now handle all remaining metadata attributes
      std::pair<FhgfsOpsErr, std::vector<std::string>> listXAttrs = XAttrTk::listXAttrs(fullPath.str());
      if (listXAttrs.first != FhgfsOpsErr_SUCCESS)
         return listXAttrs.first;

      for (auto const& attrName : listXAttrs.second)
      {
         // Process all metadata-specific attributes except META_XATTR_NAME (already handled above)
         // This approach ensures we only process attribute(s) that:
         // 1. Exist on the inode.
         // 2. Are listed in METADATA_XATTR_NAME_LIST, our collection of known metadata attributes.
         // 3. Is not META_XATTR_NAME, to prevent duplicate processing.
         if (std::find(METADATA_XATTR_NAME_LIST.begin(), METADATA_XATTR_NAME_LIST.end(), attrName)
               != METADATA_XATTR_NAME_LIST.end() && (attrName != META_XATTR_NAME))
         {
             if (!readAndStoreMetaAttribute(attrName))
                return readRes;
         }
      }
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
