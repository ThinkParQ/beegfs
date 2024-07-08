#include <common/storage/EntryInfo.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include <toolkit/XAttrTk.h>
#include "MsgHelperXAttr.h"

#include <sys/xattr.h>

const std::string MsgHelperXAttr::CURRENT_DIR_FILENAME = std::string(".");
const ssize_t MsgHelperXAttr::MAX_VALUE_SIZE = 60*1024;

std::pair<FhgfsOpsErr, StringVector> MsgHelperXAttr::listxattr(EntryInfo* entryInfo)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   if (entryInfo->getEntryType() == DirEntryType_REGULARFILE && !entryInfo->getIsInlined())
   {
      MetaFileHandle inode = metaStore->referenceFile(entryInfo);
      if (!inode)
         return {FhgfsOpsErr_PATHNOTEXISTS, {}};

      auto result = inode->listXAttr();

      metaStore->releaseFile(entryInfo->getParentEntryID(), inode);

      return result;
   }

   DirInode* dir = metaStore->referenceDir(
         DirEntryType_ISDIR(entryInfo->getEntryType())
            ? entryInfo->getEntryID()
            : entryInfo->getParentEntryID(),
         entryInfo->getIsBuddyMirrored(),
         true);

   if (!dir)
      return {FhgfsOpsErr_INTERNAL, {}};

   auto result = dir->listXAttr(
         DirEntryType_ISDIR(entryInfo->getEntryType())
            ? nullptr
            : entryInfo);

   metaStore->releaseDir(dir->getID());

   return result;
}


std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> MsgHelperXAttr::getxattr(EntryInfo* entryInfo,
   const std::string& name, size_t maxSize)
{
   std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> result;
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   if (entryInfo->getEntryType() == DirEntryType_REGULARFILE && !entryInfo->getIsInlined())
   {
      MetaFileHandle inode = metaStore->referenceFile(entryInfo);
      if (!inode)
         return std::make_tuple(FhgfsOpsErr_PATHNOTEXISTS, std::vector<char>(), ssize_t(0));

      result = inode->getXAttr(name, maxSize);

      metaStore->releaseFile(entryInfo->getParentEntryID(), inode);
   }
   else
   {
      DirInode* dir = metaStore->referenceDir(
            DirEntryType_ISDIR(entryInfo->getEntryType())
               ? entryInfo->getEntryID()
               : entryInfo->getParentEntryID(),
            entryInfo->getIsBuddyMirrored(),
            true);

      if (!dir)
         return std::make_tuple(FhgfsOpsErr_INTERNAL, std::vector<char>(), ssize_t(0));

      result = dir->getXAttr(
            DirEntryType_ISDIR(entryInfo->getEntryType())
               ? nullptr
               : entryInfo,
            name,
            maxSize);

      metaStore->releaseDir(dir->getID());
   }

   // Attribute might be too large for NetMessage.
   if (std::get<1>(result).size() > size_t(MsgHelperXAttr::MAX_VALUE_SIZE))
   {
      // Note: This can happen if it was set with an older version of the client which did not
      //       include the size check.
      return std::make_tuple(FhgfsOpsErr_INTERNAL, std::vector<char>(), ssize_t(0));
   }

   return result;
}

FhgfsOpsErr MsgHelperXAttr::removexattr(EntryInfo* entryInfo, const std::string& name)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   if (entryInfo->getEntryType() == DirEntryType_REGULARFILE && !entryInfo->getIsInlined())
   {
      MetaFileHandle inode = metaStore->referenceFile(entryInfo);
      if (!inode)
         return FhgfsOpsErr_PATHNOTEXISTS;

      auto result = inode->removeXAttr(entryInfo, name);

      metaStore->releaseFile(entryInfo->getParentEntryID(), inode);

      return result;
   }

   DirInode* dir = metaStore->referenceDir(
         DirEntryType_ISDIR(entryInfo->getEntryType())
            ? entryInfo->getEntryID()
            : entryInfo->getParentEntryID(),
         entryInfo->getIsBuddyMirrored(),
         true);

   if (!dir)
      return FhgfsOpsErr_INTERNAL;

   auto result = dir->removeXAttr(
         DirEntryType_ISDIR(entryInfo->getEntryType())
            ? nullptr
            : entryInfo,
         name);

   metaStore->releaseDir(dir->getID());

   return result;
}

FhgfsOpsErr MsgHelperXAttr::setxattr(EntryInfo* entryInfo, const std::string& name,
   const CharVector& value, int flags)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   if (entryInfo->getEntryType() == DirEntryType_REGULARFILE && !entryInfo->getIsInlined())
   {
      MetaFileHandle inode = metaStore->referenceFile(entryInfo);
      if (!inode)
         return FhgfsOpsErr_PATHNOTEXISTS;

      auto result = inode->setXAttr(entryInfo, name, value, flags);

      metaStore->releaseFile(entryInfo->getParentEntryID(), inode);

      return result;
   }

   DirInode* dir = metaStore->referenceDir(
         DirEntryType_ISDIR(entryInfo->getEntryType())
            ? entryInfo->getEntryID()
            : entryInfo->getParentEntryID(),
         entryInfo->getIsBuddyMirrored(),
         true);

   if (!dir)
      return FhgfsOpsErr_INTERNAL;

   auto result = dir->setXAttr(
         DirEntryType_ISDIR(entryInfo->getEntryType())
            ? nullptr
            : entryInfo,
         name,
         value,
         flags);

   metaStore->releaseDir(dir->getID());

   return result;
}

FhgfsOpsErr MsgHelperXAttr::StreamXAttrState::streamXattrFn(Socket* socket, void* context)
{
   StreamXAttrState* state = static_cast<StreamXAttrState*>(context);

   return state->streamXattr(socket);
}

FhgfsOpsErr MsgHelperXAttr::StreamXAttrState::streamXattr(Socket* socket) const
{
   for (auto xattr = names.cbegin(); xattr != names.cend(); ++xattr)
   {
      const auto& name = *xattr;

      CharVector value;
      FhgfsOpsErr getRes;

      if (entryInfo)
         std::tie(getRes, value, std::ignore) = getxattr(entryInfo, name, XATTR_SIZE_MAX);
      else
         std::tie(getRes, value, std::ignore) = XAttrTk::getUserXAttr(path, name, XATTR_SIZE_MAX);

      if (getRes != FhgfsOpsErr_SUCCESS)
      {
         uint32_t endMark = HOST_TO_LE_32(-1);
         socket->send(&endMark, sizeof(endMark), 0);
         return getRes;
      }

      uint32_t nameLen = HOST_TO_LE_32(name.size());
      socket->send(&nameLen, sizeof(nameLen), 0);
      socket->send(&name[0], nameLen, 0);

      uint64_t valueLen = HOST_TO_LE_64(value.size());
      socket->send(&valueLen, sizeof(valueLen), 0);
      socket->send(&value[0], value.size(), 0);
   }

   uint32_t endMark = HOST_TO_LE_32(0);
   socket->send(&endMark, sizeof(endMark), 0);

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr MsgHelperXAttr::StreamXAttrState::readNextXAttr(Socket* socket, std::string& name,
   CharVector& value)
{
   uint32_t nameLen;
   Config* cfg = Program::getApp()->getConfig();

   ssize_t nameLenRes = socket->recvExactT(&nameLen, sizeof(nameLen), 0, cfg->getConnMsgShortTimeout());
   if (nameLenRes < 0 || size_t(nameLenRes) < sizeof(nameLen))
      return FhgfsOpsErr_COMMUNICATION;

   nameLen = LE_TO_HOST_32(nameLen);
   if (nameLen == 0)
      return FhgfsOpsErr_SUCCESS;
   else if (nameLen == uint32_t(-1))
      return FhgfsOpsErr_COMMUNICATION;

   if (nameLen > XATTR_NAME_MAX)
      return FhgfsOpsErr_RANGE;

   name.resize(nameLen);
   if (socket->recvExactT(&name[0], nameLen, 0, cfg->getConnMsgShortTimeout()) != (ssize_t) name.size())
      return FhgfsOpsErr_COMMUNICATION;

   uint64_t valueLen;
   ssize_t valueLenRes = socket->recvExactT(&valueLen, sizeof(valueLen), 0,
         cfg->getConnMsgShortTimeout());
   if (valueLenRes < 0 || size_t(valueLenRes) != sizeof(valueLen))
      return FhgfsOpsErr_COMMUNICATION;

   valueLen = LE_TO_HOST_64(valueLen);
   if (valueLen > XATTR_SIZE_MAX)
      return FhgfsOpsErr_RANGE;

   value.resize(valueLen);
   if (socket->recvExactT(&value[0], valueLen, 0, cfg->getConnMsgShortTimeout()) != ssize_t(valueLen))
      return FhgfsOpsErr_COMMUNICATION;

   return FhgfsOpsErr_AGAIN;
}
