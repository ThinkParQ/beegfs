#include "Common.h"
#include "Main.h"
#include "StorageTk.h"
#include "System.h"
#include "MapTk.h"
#include "Config.h"
#include "LogContext.h"

static StringUnsignedMap globalMetaIdMap;
static bool globalAllOK = true;

static Logger* globalLogger = NULL;

/* general note: "old" as suffix means 2011.04 storage version, "new" as suffix means 2012.10
   storage version */


#define CONFIG_CHUNK_SUBDIRS_NUM_OLD      (8192)

#define CONFIG_CHUNK_LEVEL1_SUBDIR_NUM_NEW (128)
#define CONFIG_CHUNK_LEVEL2_SUBDIR_NUM_NEW (128)

#define CONFIG_CHUNK_SUBDIR_NAME          "chunks"
#define CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP  "chunks.new" /* tmp name to move chunk files */
#define CONFIG_CHUNK_SUBDIR_NAME_OLD_TMP  "chunks.old" /* tmp name to switch chunk dirs */
#define STORAGE_FORMAT_VERSION_OLD        1
#define STORAGE_FORMAT_VERSION_NEW        2


#define ID_MAP_FILE_COLUMS (2)

#define SUFFIX_LENGTH (2) // -f or -d

#define ID_SEPARATOR ('-') // dash is used as seperator

#define STRING_NOT_FOUND_RET (-1)


Logger* getGlobalLogger()
{
   return globalLogger;
}


void printUsage(std::string progName)
{
   std::cout << std::endl;
   std::cout << "FhGFS Storage Server Format Upgrade Tool (2011.04 -> 2012.10)" << std::endl;
   std::cout << "Version: " << FHGFS_VERSION << std::endl;
   std::cout << "http://www.fhgfs.com" << std::endl;
   std::cout << std::endl;
   std::cout << "* Usage: " << std::endl;
   std::cout << "     " << progName << "                \\" << std::endl;
   std::cout << "           <storeStorageDirectory=storage_directory> \\" << std::endl;
   std::cout << "           <metaIdMapFile=metaIDMapFile>             \\" << std::endl;
   std::cout << "           <targetIdMapFile=targetIDMapFile>         \\" << std::endl;
   std::cout << "           (<storageIdMapFile>=storageIdMapFile)"  << std::endl;
   std::cout << std::endl;
   std::cout << "* Example: " << std::endl;
   std::cout << "# " << progName << "                       \\" << std::endl;
   std::cout << "       storeStorageDirectory=/data/fhgfs/storage/target1 \\" << std::endl;
   std::cout << "       metaIdMapFile=/root/metaIDMapFile.txt             \\" << std::endl;
   std::cout << "       targetIdMapFile=/root/targetIDMapFile.txt         \\" << std::endl;
   std::cout << "       storageIdMapFile=/root/storageIDMapFile.txt" << std::endl;
   std::cout << std::endl;
   std::cout << "* Note: <storageIdMapFile> is optional." << std::endl;
   std::cout << std::endl;
}


bool loadIDMap(std::string mapFileName, StringUnsignedMap* outIdMap)
{
   const char* logContext = "Load IP map file";
   LogContext log(logContext);

   std::ifstream fis(mapFileName.c_str() );
   if(!fis.is_open() || fis.fail() )
   {
     log.logErr("Failed to open ID-map-file: " + System::getErrString() );

      return false;
   }

   bool retVal = true;
   char line[STORAGETK_FILE_MAX_LINE_LENGTH];
   while(!fis.eof() && !fis.fail() )
   {
      fis.getline(line, STORAGETK_FILE_MAX_LINE_LENGTH);
      std::string trimmedLine = StringTk::trim(line);

      if(trimmedLine.length() && (trimmedLine[0] != STORAGETK_FILE_COMMENT_CHAR) )
      {
         unsigned numID;
         char stringID[513];

         int scanRes = sscanf(trimmedLine.c_str(), "%48s %u", stringID, &numID);
         if (scanRes == ID_MAP_FILE_COLUMS)
            outIdMap->insert(StringUnsignedMapVal(stringID, numID) );
         else
         {
            log.logErr("Invalid line in map file (" + mapFileName + ")" +
               std::string(" Line: ") + line);
            break;
            retVal = false;
         }
      }
   }

   fis.close();

   return retVal;
}


bool mapStringToNumID(StringUnsignedMap* idMap, std::string inStrID, uint16_t& outNumID)
{
   StringUnsignedMapIter mapIter = idMap->find(inStrID);
   if (unlikely(mapIter == idMap->end() ) )
      return false; // not in map

   outNumID = mapIter->second;
   return true;

}

bool updateEntryID(std::string inEntryID,  std::string& outEntryID)
{
   const char* logContext = "Update EntryID";
   size_t firstSepPos;
   size_t secondSepPos;
   std::string stringID;
   uint16_t numID = 0;
   bool mapRes;

   StringUnsignedMap* metaIdMap = &globalMetaIdMap;

   firstSepPos = inEntryID.find(ID_SEPARATOR);
   if (firstSepPos == (size_t) STRING_NOT_FOUND_RET)
      goto outerr;

   secondSepPos = inEntryID.find(ID_SEPARATOR, firstSepPos + 1);
   if (secondSepPos == (size_t) STRING_NOT_FOUND_RET)
      goto outerr;

   stringID = inEntryID.substr(secondSepPos + 1);

   mapRes = mapStringToNumID(metaIdMap, stringID, numID);
   if (!mapRes )
      goto outerr;

   outEntryID = inEntryID.substr(0, secondSepPos + 1) + StringTk::uint16ToHexStr(numID);

   return true;

outerr:
   LogContext(logContext).logErr("Failed to update entryID: " + inEntryID);
   outEntryID = inEntryID; // make mapping error non-fatal
   globalAllOK = false;
   return false;
}



bool checkAndLockStorageDir(std::string dir)
{
   const char* logContext = "Check and lock the storage directory";
   LogContext log(logContext);

   if(!StorageTk::pathExists(dir) )
   {
      log.logErr("Directory not found: " + dir);
      return false;
   }

   if(!StorageTk::checkStorageFormatFileExists(dir) )
   {
      log.logErr("Storage format file not found in directory: " + dir);
      return false;
   }

   if(!StorageTk::checkStorageFormatFile(dir, STORAGE_FORMAT_VERSION_OLD) )
   {
      log.logErr("Invalid storage format file in directory: " + dir);
      globalAllOK = false;
      return false;
   }

   std::string chunksDir = dir + "/" + CONFIG_CHUNK_SUBDIR_NAME;
   if(!StorageTk::pathExists(chunksDir) )
   {
      log.logErr("Chunks directory not found: " + chunksDir);
      globalAllOK = false;
      return false;
   }

   std::string firstHighHashDir =
      chunksDir + "/" + StringTk::uintToHexStr(0);
   if(!StorageTk::pathExists(firstHighHashDir) )
   {
      log.logErr("Chunks hash directory not found: " + firstHighHashDir);
      globalAllOK = false;
      return false;
   }

   std::string tooHighHashDir =
      chunksDir + "/" + StringTk::uintToHexStr(CONFIG_CHUNK_SUBDIRS_NUM_OLD);
   if(StorageTk::pathExists(tooHighHashDir) )
   {
      /* note: 2011.04 fomat has hash dirs 0..8191 (in hex), so we check for hash dir 8191 */
      log.logErr("Found invalid chunks hash directory: " + tooHighHashDir);
      globalAllOK = false;
      return false;
   }

   /* (note: we will never unlock this dir, because system will do it for us on exit and we don't
      really want to care about the cleanup code in this tool.) */
   int lockFD = StorageTk::lockWorkingDirectory(dir);
   if(lockFD == -1)
   {
      log.logErr("Unable to lock storage directory:" + dir);
      globalAllOK = false;
      return false;
   }

   return true;
}

bool createChunksTmpDir(std::string storageDir)
{
   const char* logContext = "Create new chunks tmp dir";

   // chunks directory
   Path chunksPathTmp(storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP);

   if(!StorageTk::createPathOnDisk(chunksPathTmp, false) )
   {
      LogContext(logContext).logErr("Unable to create new chunks directory: " +
         chunksPathTmp.getPathAsStr() );
      globalAllOK = false;
      return false;
   }

   StorageTk::initHashPaths(chunksPathTmp, CONFIG_CHUNK_LEVEL1_SUBDIR_NUM_NEW,
      CONFIG_CHUNK_LEVEL2_SUBDIR_NUM_NEW);

   return true;
}


bool moveChunkFilesToTmpDir(std::string storageDir)
{
   const char* logContext = "Chunk re-hashing";
   LogContext log(logContext);

   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   std::string pathOld = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME;
   std::string pathNew = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP;

   log.log(Log_NOTICE, std::string("* Re-hashing chunk files... (Processing ") +
      StringTk::intToStr(CONFIG_CHUNK_SUBDIRS_NUM_OLD) + " directories...)");
    log.log(Log_NOTICE, "Progress: ");

   // walk over all old chunks subdirs
   for(unsigned i=0; i < CONFIG_CHUNK_SUBDIRS_NUM_OLD; i++)
   {
      std::string subdirNameOld = StringTk::uintToHexStr(i);
      std::string subdirPathOld = pathOld + "/" + subdirNameOld;

      log.log(Log_NOTICE, subdirNameOld + " ", true);

      DIR* dirHandle = opendir(subdirPathOld.c_str() );
      if(!dirHandle)
      {
         log.logErr("Unable to open hash directory: " + subdirPathOld + ". " +
            "SysErr: " + System::getErrString() );
         globalAllOK = false;

         return false;
      }


      errno = 0; // recommended by posix (readdir(3p) )

      // read all entries and move them to new chunks tmp dir
      while( (dirEntry = StorageTk::readdirFiltered(dirHandle) ) )
      {
         std::string entryID = dirEntry->d_name;
         std::string updatedEntryID;

         if (!updateEntryID(dirEntry->d_name, updatedEntryID) )
         {
            globalAllOK = false;
            continue;
         }

         std::string filePathOld = subdirPathOld + "/" + entryID;
         std::string filePathNew = StorageTk::getHashPath(pathNew, updatedEntryID,
            CONFIG_CHUNK_LEVEL1_SUBDIR_NUM_NEW, CONFIG_CHUNK_LEVEL2_SUBDIR_NUM_NEW);

         int renameRes = rename(filePathOld.c_str(), filePathNew.c_str() );
         if(renameRes)
         {
            log.logErr("Unable to move file: " + filePathOld + " -> " + filePathNew + ". " +
               "SysErr: " + System::getErrString() );

            globalAllOK = false;
            goto err_closedir;
         }

         numEntries++;
      }

      if(!dirEntry && errno)
      {
         log.logErr("Unable to fetch directory entry from: " + pathOld + ". " +
            "SysErr: " + System::getErrString() );

         globalAllOK = false;
         goto err_closedir;
      }

      closedir(dirHandle);

      continue;

   err_closedir:
      closedir(dirHandle);
      return false;
   }

   log.log(Log_NOTICE, "\nRe-hashed files: " + StringTk::intToStr(numEntries) );

   // all entries read/moved
   return true;
}

bool switchChunksDirToNew(std::string storageDir)
{
   const char* logContext = "switchChunksDirToNew";
   LogContext log(logContext);

   std::string pathOrig = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME;
   std::string pathNewTmp = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP;
   std::string pathOldTmp = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME_OLD_TMP;

   log.log(Log_NOTICE, "* Switching to new chunks directory...");

   int renameOldRes = rename(pathOrig.c_str(), pathOldTmp.c_str() );
   if(renameOldRes)
   {
      log.logErr("Unable to move directory: " + pathOrig + " -> " +
         pathOldTmp + ". SysErr: " + System::getErrString() );

      globalAllOK = false;
      return false;
   }

   int renameNewRes = rename(pathNewTmp.c_str(), pathOrig.c_str() );
   if(renameNewRes)
   {
      log.logErr("Unable to move directory: " + pathNewTmp + " -> " +
         pathOrig + ". SysErr: " + System::getErrString() );

      globalAllOK = false;
      return false;
   }

   log.log(Log_NOTICE, "* Cleaning up old chunks directory...");

   for(unsigned i=0; i < CONFIG_CHUNK_SUBDIRS_NUM_OLD; i++)
   {
      std::string subdirNameOld = StringTk::uintToHexStr(i);
      std::string subdirPathOld = pathOldTmp + "/" + subdirNameOld;

      int rmSubRes = rmdir(subdirPathOld.c_str() );
      if(rmSubRes)
      {
         log.logErr("Unable to remove directory: " + subdirPathOld + ". " +
            "SysErr: " + System::getErrString() );

         globalAllOK = false;
         return false;
      }
   }

   int rmParentRes = rmdir(pathOldTmp.c_str() );
   if(rmParentRes)
   {
      log.logErr("Unable to remove directory: " + pathOldTmp + ". " +
         "SysErr: " + System::getErrString() );

      globalAllOK = false;
      return false;
   }

   return true;
}


void printFinalInfo()
{
   const char* logContext = "Final Info";
   LogContext log(logContext);

   log.logErr("");

   if (globalAllOK)
   {
      log.logErr("* Format conversion complete.");
      log.logErr("");

      log.logErr("* Note: This was only the conversion of the storage directory.");
      log.logErr("  You still need  to update the packages and check new config file options.");
      log.logErr("");
   }
   else
   {
      log.logErr("* Format conversion completed with *errors*!");
      log.logErr("* Please check the log file, fix the issues and re-run the upgrade tool!");
      log.logErr("");
   }
}


int main(int argc, char** argv)
{
   // note: we expect exactly two user argument: the path to the storage directory and the
   //       idMap file

   char* progPath = argv[0];
   std::string progName = basename(progPath);

   if (((argc != 4) && (argc != 5) ) || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") )
   {
      printUsage(progName);
      return 1;
   }

   Config config(argc, argv);

   if (!config.getIsValidConfig() )
   {
      printUsage(progName);
      return 1;
   }

   globalLogger = new Logger(&config);


   LogContext log("");
   log.logErr("* Writing log file to: " + config.getLogStdFile() );
   log.logErr("  Progress may be monitored by watching this file.");

   std::string storageDir       = config.getStoreTargetDirectory();
   std::string metaIdMapFile    = config.getMetaIdMapFile();
   std::string targetIdMapFile  = config.getTargetIdMapFile();
   std::string storageIdMapFile = config.getStorageIdMapFile(); // optional

   if (!loadIDMap(metaIdMapFile, &globalMetaIdMap) )
      return 1;

   if(!checkAndLockStorageDir(storageDir) )
      return 1;

   { // map the targetID to the numTargetID
      StringUnsignedMap targetIdMap;
      if (!loadIDMap(targetIdMapFile, &targetIdMap) )
         return 1;

      std::string targetID;
      if (!StorageTk::readTargetIDFile(storageDir, &targetID) )
      {
         log.logErr("Failed to read targetID file. Aborting!");
         return 1;
      }

      uint16_t numTargetID = 0;
      if (!mapStringToNumID(&targetIdMap, targetID, numTargetID) )
      {
         log.logErr("Failed to map target string ID to numeric ID: " + targetID + ". Aborting!");
         return 1;
      }

      if (!StorageTk::createNumIDFile(storageDir, STORAGETK_TARGETNUMID_FILENAME, numTargetID) )
         return 1;
   }

   // map the storage originalNodeID
   if (!storageIdMapFile.empty() )
   {
      StringUnsignedMap storageIdMap;
      if (!loadIDMap(storageIdMapFile, &storageIdMap) )
         return 1;

      std::string nodeID;
      // first try to read the "nodeID" file
      if (!StorageTk::readNodeIDFile(storageDir, STORAGETK_NODEID_FILENAME, nodeID) )
      {
         // reading "nodeID" failed, fall back to "originalNodeID"
         if (!StorageTk::readNodeIDFile(storageDir, STORAGETK_ORIGINALNODEID_FILENAME, nodeID) )
         {
            log.logErr("Failed to read nodeID or originalNodeID file. Aborting!");
            return 1;
         }
      }

      uint16_t numNodeID = 0;
      if (!mapStringToNumID(&storageIdMap, nodeID, numNodeID) )
      {
         log.logErr("Failed to map node string ID to numeric ID: " + nodeID + ". Aborting!");
         return 1;
      }

      if (!StorageTk::createNumIDFile(storageDir, STORAGETK_NODENUMID_FILENAME, numNodeID) )
         return 1;

   }

   log.logErr("* Starting the upgrade process: ...");

   if(!createChunksTmpDir(storageDir) )
      return 1;

   if(!moveChunkFilesToTmpDir(storageDir) )
      return 1;

   if(globalAllOK)
      if (!switchChunksDirToNew(storageDir) )
      return 1;

   // storage format file
   if(!StorageTk::createStorageFormatFile(storageDir, STORAGE_FORMAT_VERSION_NEW) )
      return 1;


   printFinalInfo();

   return 0;
}
