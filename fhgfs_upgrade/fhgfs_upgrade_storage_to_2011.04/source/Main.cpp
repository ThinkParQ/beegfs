#include "Common.h"
#include "Main.h"
#include "StorageTk.h"
#include "System.h"
#include "MapTk.h"


/* general note: "old" as suffix means 2009.08 storage version, "new" as suffix means 2011.04
   storage version */


#define CONFIG_CHUNK_SUBDIRS_NUM_OLD      (1024)
#define CONFIG_CHUNK_SUBDIRS_NUM_NEW      (8192)
#define CONFIG_CHUNK_SUBDIR_NAME          "chunks"
#define CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP  "chunks.new" /* tmp name to move chunk files */
#define CONFIG_CHUNK_SUBDIR_NAME_OLD_TMP  "chunks.old" /* tmp name to switch chunk dirs */
#define CONFIG_NODEID_FILENAME            "nodeID"
#define CONFIG_TARGETID_FILENAME          "targetID"
#define STORAGE_FORMAT_VERSION_OLD        1


void printUsage(std::string progName)
{
   std::cout << "FhGFS Storage Server Format Upgrade Tool (2009.08 -> 2011.04)" << std::endl;
   std::cout << "Version: " << FHGFS_VERSION << std::endl;
   std::cout << "http://www.fhgfs.com" << std::endl;
   std::cout << std::endl;
   std::cout << "* Usage..: " << progName << " <storage_directory>" << std::endl;
   std::cout << std::endl;
   std::cout << "* Example: " << progName << " /data/fhgfs_storage" << std::endl;
   std::cout << std::endl;
   std::cout << "BACKUP YOUR DATA BEFORE STARTING THE UPGRADE PROCESS!" << std::endl;
   std::cout << std::endl;
}

bool checkAndLockStorageDir(std::string dir)
{
   std::cout << "* Checking storage directory..." << std::endl;

   if(!StorageTk::pathExists(dir) )
   {
      std::cerr << "Directory not found: " << dir << std::endl;
      return false;
   }

   if(!StorageTk::checkStorageFormatFileExists(dir) )
   {
      std::cerr << "Storage format file not found in directory: " << dir << std::endl;
      return false;
   }

   if(!StorageTk::checkStorageFormatFile(dir, STORAGE_FORMAT_VERSION_OLD) )
   {
      std::cerr << "Invalid storage format file in directory: " << dir << std::endl;
      return false;
   }

   std::string chunksDir = dir + "/" + CONFIG_CHUNK_SUBDIR_NAME;
   if(!StorageTk::pathExists(chunksDir) )
   {
      std::cerr << "Chunks directory not found: " << chunksDir << std::endl;
      return false;
   }

   std::string firstHighHashDir =
      chunksDir + "/" + StringTk::uintToHexStr(0);
   if(!StorageTk::pathExists(firstHighHashDir) )
   {
      std::cerr << "Chunks hash directory not found: " << firstHighHashDir << std::endl;
      return false;
   }

   std::string tooHighHashDir =
      chunksDir + "/" + StringTk::uintToHexStr(CONFIG_CHUNK_SUBDIRS_NUM_OLD);
   if(StorageTk::pathExists(tooHighHashDir) )
   {
      /* note: 2009.08 fomat has only hash dirs 0..1023 (in hex), so we check for hash dir 1024,
         which shouldn't exist here (but was introduced in the 2011.04 storage format) */
      std::cerr << "Found invalid chunks hash directory: " << tooHighHashDir << std::endl;
      return false;
   }

   /* (note: we will never unlock this dir, because system will do it for us on exit and we don't
      really want to care about the cleanup code in this tool.) */
   int lockFD = StorageTk::lockWorkingDirectory(dir);
   if(lockFD == -1)
   {
      std::cerr << "Unable to lock storage directory:" << dir << std::endl;
      return false;
   }

   return true;
}

bool createChunksTmpDir(std::string storageDir)
{
   std::cout << "* Creating new chunks directory... (" << CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP << ")" <<
      std::endl;

   // chunks directory
   Path chunksPathTmp(storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP);

   if(!StorageTk::createPathOnDisk(chunksPathTmp, false) )
   {
      std::cerr << "Unable to create new chunks directory: " << chunksPathTmp.getPathAsStr() <<
         std::endl;
      return false;
   }

   // chunks subdirs
   for(unsigned i=0; i < CONFIG_CHUNK_SUBDIRS_NUM_NEW; i++)
   {
      std::string subdirName = StringTk::uintToHexStr(i);
      Path subdirPath(chunksPathTmp, subdirName);

      if(!StorageTk::createPathOnDisk(subdirPath, false) )
      {
         std::cerr << "Unable to create new chunks hash directory: " << subdirPath.getPathAsStr() <<
            std::endl;
         return false;
      }
   }

   return true;
}


bool moveChunkFilesToTmpDir(std::string storageDir)
{
   uint64_t numEntries = 0;
   struct dirent* dirEntry = NULL;

   std::string pathOld = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME;
   std::string pathNew = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP;

   std::cout << "* Re-hashing chunk files... (Processing " << CONFIG_CHUNK_SUBDIRS_NUM_OLD <<
      " directories...)" << std::endl;
   std::cout << "Progress: ";

   // walk over all old chunks subdirs
   for(unsigned i=0; i < CONFIG_CHUNK_SUBDIRS_NUM_OLD; i++)
   {
      std::string subdirNameOld = StringTk::uintToHexStr(i);
      std::string subdirPathOld = pathOld + "/" + subdirNameOld;

      std::cout << subdirNameOld << " " << std::flush; // print dir num as progress info

      DIR* dirHandle = opendir(subdirPathOld.c_str() );
      if(!dirHandle)
      {
         std::cerr << "Unable to open hash directory: " << subdirPathOld << ". " <<
            "SysErr: " << System::getErrString() << std::endl;

         return false;
      }


      errno = 0; // recommended by posix (readdir(3p) )

      // read all entries and move them to new chunks tmp dir
      while( (dirEntry = StorageTk::readdirFiltered(dirHandle) ) )
      {
         std::string filename = dirEntry->d_name;
         unsigned nameChecksum = StringTk::strChecksum(filename.c_str(), filename.length() )
            % CONFIG_CHUNK_SUBDIRS_NUM_NEW;

         std::string filePathOld = subdirPathOld + "/" + filename;
         std::string filePathNew = pathNew + "/" + StringTk::uintToHexStr(nameChecksum) + "/" +
            filename;

         int renameRes = rename(filePathOld.c_str(), filePathNew.c_str() );
         if(renameRes)
         {
            std::cerr << "Unable to move file: " << filePathOld << " -> " << filePathNew << ". " <<
               "SysErr: " << System::getErrString() << std::endl;

            goto err_closedir;
         }

         numEntries++;
      }

      if(!dirEntry && errno)
      {
         std::cerr << "Unable to fetch directory entry from: " << pathOld << ". " <<
            "SysErr: " << System::getErrString() << std::endl;

         goto err_closedir;
      }

      closedir(dirHandle);

      continue;

   err_closedir:
      closedir(dirHandle);
      return false;
   }


   std::cout << std::endl; // finalizing endl for dir progress

   std::cout << "Re-hashed files: " << numEntries << std::endl;

   // all entries read/moved
   return true;
}

bool switchChunksDirToNew(std::string storageDir)
{
   std::string pathOrig = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME;
   std::string pathNewTmp = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME_NEW_TMP;
   std::string pathOldTmp = storageDir + "/" CONFIG_CHUNK_SUBDIR_NAME_OLD_TMP;

   std::cout << "* Switching to new chunks directory..." << std::endl;

   int renameOldRes = rename(pathOrig.c_str(), pathOldTmp.c_str() );
   if(renameOldRes)
   {
      std::cerr << "Unable to move directory: " << pathOrig << " -> " <<
         pathOldTmp << ". SysErr: " << System::getErrString() << std::endl;

      return false;
   }

   int renameNewRes = rename(pathNewTmp.c_str(), pathOrig.c_str() );
   if(renameNewRes)
   {
      std::cerr << "Unable to move directory: " << pathNewTmp << " -> " <<
         pathOrig << ". SysErr: " << System::getErrString() << std::endl;

      return false;
   }

   std::cout << "* Cleaning up old chunks directory..." << std::endl;

   for(unsigned i=0; i < CONFIG_CHUNK_SUBDIRS_NUM_OLD; i++)
   {
      std::string subdirNameOld = StringTk::uintToHexStr(i);
      std::string subdirPathOld = pathOldTmp + "/" + subdirNameOld;

      int rmSubRes = rmdir(subdirPathOld.c_str() );
      if(rmSubRes)
      {
         std::cerr << "Unable to remove directory: " << subdirPathOld << ". " <<
            "SysErr: " << System::getErrString() << std::endl;

         return false;
      }
   }

   int rmParentRes = rmdir(pathOldTmp.c_str() );
   if(rmParentRes)
   {
      std::cerr << "Unable to remove directory: " << pathOldTmp << ". " <<
         "SysErr: " << System::getErrString() << std::endl;

      return false;
   }

   return true;
}

bool createTargetIDFile(std::string storageDir)
{
   std::string nodeIDPath = storageDir + "/" + CONFIG_NODEID_FILENAME;
   std::string targetIDPath = storageDir + "/" + CONFIG_TARGETID_FILENAME;
   std::string targetID;

   std::cout << "* Creating target ID... ";

   if(StorageTk::pathExists(nodeIDPath) )
   {
      StringMap nodeIDMap;
      bool loadRes = MapTk::loadStringMapFromFile(nodeIDPath.c_str(), &nodeIDMap);
      if(!loadRes)
      {
         std::cerr << "Unable to load nodeID file: " << nodeIDPath << std::endl;
         return false;
      }

      if(!nodeIDMap.empty() )
         targetID = nodeIDMap.begin()->first;
   }

   if(targetID.empty() )
      targetID = System::getHostname();


   std::cout << "TargetID: " << targetID << std::endl;


   StringMap targetIDMap;
   targetIDMap.insert(StringMapVal(targetID, "") );

   bool saveRes = MapTk::saveStringMapToFile(targetIDPath.c_str(), &targetIDMap);
   if(!saveRes)
   {
      std::cerr << "Unable to save targetID file: " << targetIDPath << std::endl;
      return false;
   }

   return true;
}

void printFinalInfo()
{
   std::cout << std::endl;
   std::cout << "* Format conversion complete." << std::endl;
   std::cout << std::endl;

   std::cout << "* Note: This was only the conversion of the storage directory. You still need " <<
      "to uninstall the FhGFS 2009.08 packages, install the new packages and set the config " <<
      "file options." << std::endl;

   std::cout << std::endl;
}


int main(int argc, char** argv)
{
   // note: we expect exactly one user argument: the path to the storage directory

   if( (argc != 2) || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") )
   {
      char* progPath = argv[0];
      std::string progName = basename(progPath);

      printUsage(progName);
      return 1;
   }

   char* storageDir = argv[1];

   if(!checkAndLockStorageDir(storageDir) )
      return 1;

   if(!createChunksTmpDir(storageDir) )
      return 1;

   if(!moveChunkFilesToTmpDir(storageDir) )
      return 1;

   if(!createTargetIDFile(storageDir) )
      return 1;

   if(!switchChunksDirToNew(storageDir) )
      return 1;

   printFinalInfo();

   return 0;
}
