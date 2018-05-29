#include <app/App.h>
#include <common/toolkit/MetadataTk.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/UnitTk.h>
#include <toolkit/IoctlTk.h>
#include <program/Program.h>
#include "ModeIoctl.h"


#define MODEIOCTL_ARG_IOCTLTYPE                 "--type"
#define MODEIOCTL_ARG_IOCTLTYPE_GETCFGFILE      "getconfigfile"
#define MODEIOCTL_ARG_IOCTLTYPE_GETRUNTIMECFG   "getruntimeconfigfile"
#define MODEIOCTL_ARG_IOCTLTYPE_GETMOUNTID      "getmountid"
#define MODEIOCTL_ARG_IOCTLTYPE_TESTISBEEGFS    "testisbeegfs"
#define MODEIOCTL_ARG_IOCTLTYPE_GETSTRIPEINFO      "getstripeinfo"
#define MODEIOCTL_ARG_IOCTLTYPE_GETSTRIPETARGETS   "getstripetargets"
#define MODEIOCTL_ARG_IOCTLTYPE_MKFILE             "mkfile"
#define MODEIOCTL_ARG_IOCTLTYPE_GETINODEID         "getinodeid"
#define MODEIOCTL_ARG_IOCTLTYPE_GETENTRYINFO       "getentryinfo"

#define MODEIOCTL_ARG_MKFILE_MODE                  "--mode"
#define MODEIOCTL_ARG_MKFILE_NUMTARGETS            "--numtargets"
#define MODEIOCTL_ARG_MKFILE_CHUNKSIZE             "--chunksize"
#define MODEIOCTL_ARG_MKFILE_FILENAME              "--filename"

#define MODEIOCTL_ARG_GETINODEID_ENTRYID           "--entryid"


int ModeIoctl::execute()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   StringMapIter iter; // config iterator


   // check command-line arguments...

   // ioctl-type argument

   std::string ioctlTypeStr;
   iter = cfg->find(MODEIOCTL_ARG_IOCTLTYPE);
   if(iter != cfg->end() )
   {
      ioctlTypeStr = iter->second;
      cfg->erase(iter);
   }


   // special handling for complex ioctls, which take extra arguments...

   if(ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_MKFILE)
   {
      return ioctlMkFileWithStripeHints();
   }
   else if (ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_GETINODEID)
   {
      return ioctlGetInodeID();
   }

   // here we have the generic/simple ioctls, which take no extra arguments...

   // path argument

   if(cfg->empty() )
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   std::string pathStr = cfg->begin()->first;
   cfg->erase(cfg->begin() );


   // remaining invalid arguments

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;


   // open provided path

   IoctlTk ioctlTk(pathStr);

   if(!ioctlTk.isFDValid() )
   {
      std::cerr << "Invalid path: " << pathStr << " "
         "(SysErr: " << ioctlTk.getErrMsg() << ")" << std::endl;

      return APPCODE_INVALID_CONFIG;
   }


   // run the actual work-horse depending on given ioctl type...

   if(ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_GETMOUNTID)
      return ioctlGetMountID(ioctlTk);
   else
   if(ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_GETCFGFILE)
      return ioctlGetCfgFile(ioctlTk);
   else
   if(ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_GETRUNTIMECFG)
      return ioctlGetRuntimeCfgFile(ioctlTk);
   else
   if(ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_TESTISBEEGFS)
      return ioctlTestIsFhgfs(ioctlTk);
   else
   if(ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_GETSTRIPEINFO)
      return ioctlGetStripeInfo(ioctlTk);
   else
   if(ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_GETSTRIPETARGETS)
      return ioctlGetStripeTargets(ioctlTk);
   else
   if(ioctlTypeStr == MODEIOCTL_ARG_IOCTLTYPE_GETENTRYINFO)
      return ioctlGetEntryInfo(ioctlTk);

   // invalid/missing ioctl type
   std::cerr << "Invalid or missing ioctl type." << std::endl;
   return APPCODE_INVALID_CONFIG;
}

void ModeIoctl::printHelp()
{
   std::cout << "MODE ARGUMENTS" << std::endl;
   std::cout << "==============" << std::endl;
   std::cout << " Mandatory:" << std::endl;
   std::cout << "  <path>                 Path to a file or directory inside a BeeGFS mount." << std::endl;
   std::cout << "  --type=<type>          Type of ioctl to use with the given path." << std::endl;
   std::cout << "                         (Types: getconfigfile, getruntimeconfigfile," << std::endl;
   std::cout << "                         getmountid, testisbeegfs, getstripeinfo," << std::endl;
   std::cout << "                         getstripetargets)" << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE EXAMPLE" << std::endl;
   std::cout << "=============" << std::endl;
   std::cout << " Example: Retrieve runtime config file from BeeGFS mountpoint" << std::endl;
   std::cout << "  $ beegfs-ctl --ioctl --type=getruntimeconfigfile /mnt/beegfs" << std::endl;
}

int ModeIoctl::ioctlGetMountID(IoctlTk& ioctlTk)
{
   // client mountID
   std::string mountID;

   bool getIDRes = ioctlTk.getMountID(&mountID);
   if(!getIDRes)
   {
      ioctlTk.printErrMsg();
      return APPCODE_RUNTIME_ERROR;
   }

   std::cout << mountID << std::endl;

   return APPCODE_NO_ERROR;
}

int ModeIoctl::ioctlGetCfgFile(IoctlTk& ioctlTk)
{
   // client config file
   std::string cfgFile;

   bool getRes = ioctlTk.getCfgFile(&cfgFile);
   if(!getRes)
   {
      ioctlTk.printErrMsg();
      return APPCODE_RUNTIME_ERROR;
   }

   std::cout << cfgFile << std::endl;

   return APPCODE_NO_ERROR;
}

int ModeIoctl::ioctlGetRuntimeCfgFile(IoctlTk& ioctlTk)
{
   // client config file
   std::string cfgFile;

   bool getRes = ioctlTk.getRuntimeCfgFile(&cfgFile);
   if(!getRes)
   {
      ioctlTk.printErrMsg();
      return APPCODE_RUNTIME_ERROR;
   }

   std::cout << cfgFile << std::endl;

   return APPCODE_NO_ERROR;
}

int ModeIoctl::ioctlTestIsFhgfs(IoctlTk& ioctlTk)
{
   bool getRes = ioctlTk.testIsFhGFS();
   if(!getRes)
   {
      std::cout << "no" << std::endl;
      return APPCODE_INVALID_RESULT;
   }

   std::cout << "yes" << std::endl;

   return APPCODE_NO_ERROR;
}

int ModeIoctl::ioctlGetStripeInfo(IoctlTk& ioctlTk)
{
   unsigned patternType;
   unsigned chunkSize;
   uint16_t numTargets;

   bool getRes = ioctlTk.getStripeInfo(&patternType, &chunkSize, &numTargets);

   if(!getRes)
   {
      ioctlTk.printErrMsg();
      return APPCODE_RUNTIME_ERROR;
   }

   std::cout << "PatternType: " << patternType << std::endl;
   std::cout << "PatternName: "
      << StripePattern::getPatternTypeStr( (StripePatternType)patternType) << std::endl;
   std::cout << "ChunkSize: " << chunkSize << std::endl;
   std::cout << "NumTargets: " << numTargets << std::endl;

   return APPCODE_NO_ERROR;
}

int ModeIoctl::ioctlGetStripeTargets(IoctlTk& ioctlTk)
{
   unsigned patternType;
   unsigned chunkSize;
   uint16_t numTargets;

   // retrieve number of targets through other ioctl...

   bool getRes = ioctlTk.getStripeInfo(&patternType, &chunkSize, &numTargets);

   if(!getRes)
   {
      ioctlTk.printErrMsg();
      return APPCODE_RUNTIME_ERROR;
   }

   // retrieve the individual stripe targets...

   for (uint32_t i=0; i < numTargets; i++)
   {
      BeegfsIoctl_GetStripeTargetV2_Arg targetInfo;

      bool getRes = ioctlTk.getStripeTarget(i, targetInfo);
      if(!getRes)
      {
         ioctlTk.printErrMsg();
         return APPCODE_RUNTIME_ERROR;
      }

      if (targetInfo.primaryTarget == 0)
      {
         std::cout << targetInfo.targetOrGroup << " @ " << targetInfo.primaryNodeStrID << " "
            "[ID: " << targetInfo.primaryNodeID << "]" << std::endl;
      }
      else
      {
         std::cout << targetInfo.targetOrGroup << " @ ("
            << targetInfo.primaryTarget << " @ " << targetInfo.primaryNodeStrID <<
               " [ID: " << targetInfo.primaryNodeStrID << "], "
            << targetInfo.secondaryTarget << " @ " << targetInfo.secondaryNodeStrID <<
               " [ID: " << targetInfo.secondaryNodeID << "])" << std::endl;
      }
   }

   return APPCODE_NO_ERROR;
}

/**
 * note: given ioctl path is parent dir for the new file.
 */
int ModeIoctl::ioctlMkFileWithStripeHints()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();

   StringMapIter iter; // config iterator

   std::string filename;
   unsigned numtargets = 0;
   unsigned chunksize = 0;
   mode_t mode = 0644;


   iter = cfg->find(MODEIOCTL_ARG_MKFILE_FILENAME);
   if(iter != cfg->end() )
   {
      filename = iter->second;
      cfg->erase(iter);
   }
   else
   {
      std::cerr << "No filename given." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   iter = cfg->find(MODEIOCTL_ARG_MKFILE_MODE);
   if(iter != cfg->end() )
   {
      mode = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOCTL_ARG_MKFILE_NUMTARGETS);
   if(iter != cfg->end() )
   {
      numtargets = StringTk::strToUInt(iter->second);
      cfg->erase(iter);
   }

   iter = cfg->find(MODEIOCTL_ARG_MKFILE_CHUNKSIZE);
   if(iter != cfg->end() )
   {
      chunksize = UnitTk::strHumanToInt64(iter->second);
      cfg->erase(iter);
   }

   // path argument

   if(cfg->empty() )
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   std::string pathStr = cfg->begin()->first;
   cfg->erase(cfg->begin() );


   // check remaining invalid arguments

   if(ModeHelper::checkInvalidArgs(cfg) )
      return APPCODE_INVALID_CONFIG;


   // open provided path

   IoctlTk ioctlTk(pathStr);

   if(!ioctlTk.isFDValid() )
   {
      std::cerr << "Invalid path: " << pathStr << " "
         "(SysErr: " << ioctlTk.getErrMsg() << ")" << std::endl;

      return APPCODE_INVALID_CONFIG;
   }


   bool mkRes = ioctlTk.mkFileWithStripeHints(filename, mode, numtargets, chunksize);

   if(!mkRes)
   {
      ioctlTk.printErrMsg();
      return APPCODE_RUNTIME_ERROR;
   }

   return APPCODE_NO_ERROR;
}

int ModeIoctl::ioctlGetInodeID()
{
   App* app = Program::getApp();
   StringMap* cfg = app->getConfig()->getUnknownConfigArgs();
   StringMapIter iter;

   iter = cfg->find(MODEIOCTL_ARG_GETINODEID_ENTRYID);
   if (iter == cfg->end())
   {
      std::cerr << "No entryID given." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   const std::string entryID = iter->second;
   cfg->erase(iter);

   // path argument
   if (cfg->empty())
   {
      std::cerr << "No path specified." << std::endl;
      return APPCODE_INVALID_CONFIG;
   }

   std::string pathStr = cfg->begin()->first;
   cfg->erase(cfg->begin() );

   // check remaining invalid arguments
   if (ModeHelper::checkInvalidArgs(cfg))
      return APPCODE_INVALID_CONFIG;

   IoctlTk ioctlTk(pathStr);

   uint64_t inodeID;
   bool res = ioctlTk.getInodeID(entryID, inodeID);
   if (!res)
   {
      ioctlTk.printErrMsg();
      return APPCODE_RUNTIME_ERROR;
   }

   std::cout << inodeID << std::endl;
   return APPCODE_NO_ERROR;
}

int ModeIoctl::ioctlGetEntryInfo(IoctlTk& ioctlTk)
{
   EntryInfo entryInfo;

   if(!ioctlTk.getEntryInfo(entryInfo))
   {
      ioctlTk.printErrMsg();
      return APPCODE_RUNTIME_ERROR;
   }

   std::cout << "OwnerNodeID: " << entryInfo.getOwnerNodeID() << std::endl;
   std::cout << "ParentEntryID: " << entryInfo.getParentEntryID() << std::endl;
   std::cout << "EntryID: " << entryInfo.getEntryID() << std::endl;
   std::cout << "EntryType: " << entryInfo.getEntryType() << std::endl;
   std::cout << "FeatureFlags: " << entryInfo.getFeatureFlags() << std::endl;

   return APPCODE_NO_ERROR;
}