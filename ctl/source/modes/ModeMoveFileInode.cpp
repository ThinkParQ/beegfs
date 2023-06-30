#include <app/App.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "ModeMoveFileInode.h"

#define MODEMOVEFILEINODE_ARG_OPERATION      "--operation"
#define MODEMOVEFILEINODE_ARG_UNMOUNTEDPATH  "--unmounted"

int ModeMoveFileInode::execute()
{
   int retVal = APPCODE_RUNTIME_ERROR;

   App* app = Program::getApp();
   NodeStoreServers* metaNodes = app->getMetaNodes();
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = app->getMetaMirrorBuddyGroupMapper();

   if (!ModeHelper::checkRootPrivileges())
      return APPCODE_RUNTIME_ERROR;

   if (!processCLArgs())
   {
      return APPCODE_RUNTIME_ERROR;
   }

   NodeHandle parentDirOwnerNode;
   NodeHandle fileOwnerNode;
   EntryInfo parentDirInfo;
   EntryInfo fileInfo;

   auto dirname = path->dirname();
   if (!ModeHelper::getEntryAndOwnerFromPath(dirname, useMountedPath,
         *metaNodes, app->getMetaRoot(), *metaBuddyGroupMapper, parentDirInfo, parentDirOwnerNode))
   {
      std::cerr << "Unable to get parent directory's entryInfo. dirpath: " << dirname.str() << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   if (!ModeHelper::getEntryAndOwnerFromPath(*path, useMountedPath, *metaNodes,
         app->getMetaRoot(), *metaBuddyGroupMapper, fileInfo, fileOwnerNode))
   {
      std::cerr << "Unable to get file's entryInfo. filepath: " << path->str() << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   // check file's parent is valid or not
   if (fileInfo.getParentEntryID() != parentDirInfo.getEntryID())
   {
      std::cerr << "No such file exists." << std::endl;
      return APPCODE_RUNTIME_ERROR;
   }

   // ModeHelper::getEntryAndOwnerFromPath() doesn't give entryName but we need that in MoveFileInodeMsg
   // so create another EntryInfo object and set entryName using provided path
   EntryInfo updatedFileInfo;
   updatedFileInfo.set(
      fileInfo.getOwnerNodeID(),
      fileInfo.getParentEntryID(),
      fileInfo.getEntryID(),
      path->back(),
      fileInfo.getEntryType(),
      fileInfo.getFeatureFlags() 
   );

   if (communicate(&parentDirInfo, &updatedFileInfo))
   {
      std::cout << "Operation succeeded." << std::endl;
      retVal = APPCODE_NO_ERROR;
   }

   return retVal;
}

void ModeMoveFileInode::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                    i         Path of file on which to perform operation." << std::endl;
   std::cout << "  --operation=<deinline|reinline>     Operation which needs to be performed."      << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --unmounted                         If this is specified then the given path is relative" << std::endl;
   std::cout << "                                      to the root directory of a possibly unmounted BeeGFS." << std::endl;
   std::cout << "                                      (Symlinks will not be resolved in this case.)" << std::endl;
   std::cout << " USAGE:" << std::endl;
   std::cout << "  This mode is used to perform file Inode deinline or reinline on demand." << std::endl;
   std::cout << " Example:" << std::endl;
   std::cout << "  Deinline/Reinline input file's inode" << std::endl;
   std::cout << "  # beegfs-ctl --movefileinode --operation=deinline /mnt/beegfs/dir1/someFile.txt" << std::endl;
}

bool ModeMoveFileInode::processCLArgs()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   StringMapIter iter;

   iter = cfg->find(MODEMOVEFILEINODE_ARG_OPERATION);
   if (iter == cfg->end())
   {
      std::cerr << "No operation specified." << std::endl;
      return false;
   }
   else
   {
      std::string operation = StringTk::trim(iter->second);
      if (!strcasecmp(operation.c_str(), "deinline"))
      {
         moveMode = MODE_DEINLINE;
      }
      else if (!strcasecmp(operation.c_str(), "reinline"))
      {
         moveMode = MODE_REINLINE;
      }
      else
      {
         std::cerr << "Invalid operation specified." << std::endl;
         return false;
      }

      cfg->erase(iter);
   }

   useMountedPath = true;
   iter = cfg->find(MODEMOVEFILEINODE_ARG_UNMOUNTEDPATH);
   if (iter != cfg->end())
   {
      useMountedPath = false;
      cfg->erase(iter);
   }

   iter = cfg->begin();
   if (iter == cfg->end())
   {
      std::cerr << "No path specified." << std::endl;
      return false;
   }
   else
   {
      path.reset(new Path(iter->first));

      if (path->empty())
      {
         std::cerr << "Invalid path specified." << std::endl;
         return false;
      }
   }

   return true;
}

bool ModeMoveFileInode::communicate(EntryInfo* parentDirInfo, EntryInfo* fileInfo)
{
   MoveFileInodeRespMsg* respMsgCast;
   FhgfsOpsErr res;

   MoveFileInodeMsg msg(fileInfo, moveMode);

   NumNodeID ownerNodeID = fileInfo->getOwnerNodeID();
   RequestResponseArgs rrArgs(NULL, &msg, NETMSGTYPE_MoveFileInodeResp);
   RequestResponseNode rrNode(ownerNodeID, Program::getApp()->getMetaNodes());

   if (fileInfo->getIsBuddyMirrored())
      rrNode.setMirrorInfo(Program::getApp()->getMetaMirrorBuddyGroupMapper(), false);

   FhgfsOpsErr resp = MessagingTk::requestResponseNode(&rrNode, &rrArgs);
   if (resp != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Communication with server failed: " << ownerNodeID.str() << std::endl;
      return false;
   }

   respMsgCast = (MoveFileInodeRespMsg*) rrArgs.outRespMsg.get();
   res = (FhgfsOpsErr)respMsgCast->getResult();

   if (res != FhgfsOpsErr_SUCCESS)
   {
      std::cerr << "Node encountered an error: " << res << std::endl;
      return false;
   }

   return true;   
}
