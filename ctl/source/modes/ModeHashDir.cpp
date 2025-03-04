#include <app/App.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/MetaStorageTk.h>
#include <program/Program.h>
#include "ModeHashDir.h"

#define ModeHashDir_ARG_TYPE     "--type"
#define ModeHashDir_TYPE_INODE   "inode"
#define ModeHashDir_TYPE_DENTRY  "dentry"

enum ModehashDirType
{
   ModehashDirType_INODE,
   ModehashDirType_DENTRY
};

/**
 * Main entry method to get and print the statistics
 */
int ModeHashDir::execute()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   ModehashDirType hashType;
   std::string ID;
   std::string hashDir;
   int retVal = APPCODE_NO_ERROR;

   StringMapIter iter;

   iter = cfg->find(ModeHashDir_ARG_TYPE);
   if(iter != cfg->end() )
   {
      std::string typeStr = iter->second;
      cfg->erase(iter);

      if(!strcasecmp(typeStr.c_str(), ModeHashDir_TYPE_INODE) )
         hashType = ModehashDirType_INODE;
      else
      if(!strcasecmp(typeStr.c_str(), ModeHashDir_TYPE_DENTRY) )
         hashType = ModehashDirType_DENTRY;
      else
      {
         std::cerr << "Unknown hash dir type: " << typeStr << std::endl;
         return APPCODE_INVALID_CONFIG;
      }
   }
   else
   {
      std::cerr << "Missing option: hash dir type!" << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   iter = cfg->begin();
   if(iter != cfg->end() )
   {
      ID = iter->first;

      cfg->erase(iter);
   }
   else
   {
      std::cerr << "Missing option: ID!" << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   switch (hashType)
   {
      case ModehashDirType_INODE:
      {
         hashDir = MetaStorageTk::getMetaInodePath(META_INODES_SUBDIR_NAME, ID);
      } break;

      case ModehashDirType_DENTRY:
      {
         hashDir = MetaStorageTk::getMetaDirEntryPath(META_DENTRIES_SUBDIR_NAME, ID);
      } break;
   }

   std::cout << "Hash dir: " << hashDir << std::endl;

   return retVal;
}


/**
 * Just print the help
 */
void ModeHashDir::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  --type=<type>          The ID type to query (inode, dentry)." << std::endl;
   std::cout << "  <ID>                   The ID to calculate the path for." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " Get dentry or inode path " << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Print an inode path" << std::endl;
   std::cout << "  $ beegfs-ctl --hashdir --type=inode 4C08-51FBAC63-7C" << std::endl;
}


