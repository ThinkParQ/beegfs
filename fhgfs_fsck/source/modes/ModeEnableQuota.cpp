#include "ModeEnableQuota.h"

#include <common/toolkit/ListTk.h>
#include <common/toolkit/UnitTk.h>
#include <components/worker/AdjustChunkPermissionsWork.h>
#include <net/msghelpers/MsgHelperRepair.h>
#include <toolkit/FsckTkEx.h>

#include <program/Program.h>

ModeEnableQuota::ModeEnableQuota()
 : log("ModeEnableQuota")
{
}

void ModeEnableQuota::printHelp()
{
   std::cout << "MODE ARGUMENTS:" << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --databasePath=<path>  Path to store the database files." << std::endl;
   std::cout << "                         (Default: " << CONFIG_DEFAULT_DBPATH << ")" << std::endl;
   std::cout << "  --overwriteDbFile      Overwrite an existing database file without prompt." << std::endl;
   std::cout << "  --logOutFile=<path>    Path to the fsck output file, which contains a copy of" << std::endl;
   std::cout << "                         the console output." << std::endl;
   std::cout << "                         (Default: " << CONFIG_DEFAULT_OUTFILE << ")" << std::endl;
   std::cout << "  --logStdFile=<path>    Path to the program error log file, which contains e.g." << std::endl;
   std::cout << "                         network error messages." << std::endl;
   std::cout << "                         (Default: " << CONFIG_DEFAULT_LOGFILE << ")" << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This mode sets quota information on an existing file system." << std::endl;
   std::cout << std::endl;
   std::cout << " This is useful when quota support is being enabled on a file system instance" << std::endl;
   std::cout << " that was previously used without quota support." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Set quota information" << std::endl;
   std::cout << "  $ beegfs-fsck --enablequota" << std::endl;
}

int ModeEnableQuota::execute()
{
   App* app = Program::getApp();
   Config *cfg = app->getConfig();

   if(this->checkInvalidArgs(cfg->getUnknownConfigArgs()))
      return APPCODE_INVALID_CONFIG;

   FsckTkEx::printVersionHeader(false);
   printHeaderInformation();

   if ( !FsckTkEx::checkReachability() )
      return APPCODE_COMMUNICATION_ERROR;

   fixPermissions();

   return APPCODE_NO_ERROR;
}

void ModeEnableQuota::printHeaderInformation()
{
   Config* cfg = Program::getApp()->getConfig();

   // get the current time and create some nice output, so the user can see at which time the run
   // was started (especially nice to find older runs in the log file)
   time_t t;
   time(&t);
   std::string timeStr = std::string(ctime(&t));
   FsckTkEx::fsckOutput(
      "Started BeeGFS fsck in enableQuota mode [" + timeStr.substr(0, timeStr.length() - 1)
         + "]\nLog will be written to " + cfg->getLogStdFile()
         + "\nDatabase files will be saved in " + cfg->getDatabasePath(),
         OutputOptions_LINEBREAK | OutputOptions_HEADLINE);
}

void ModeEnableQuota::fixPermissions()
{
   SynchronizedCounter finishedWork;
   unsigned generatedWork = 0;

   AtomicUInt64 fileCount;
   AtomicUInt64 errorCount;

   NodeStore* metaNodes = Program::getApp()->getMetaNodes();

   auto nodes = metaNodes->referenceAllNodes();

   for (auto iter = nodes.begin(); iter != nodes.end(); iter++)
   {
      generatedWork++;
      Program::getApp()->getWorkQueue()->addIndirectWork(new AdjustChunkPermissionsWork(**iter,
         &finishedWork, &fileCount, &errorCount));
   }

   FsckTkEx::fsckOutput("Processed 0 entries", OutputOptions_ADDLINEBREAKBEFORE |
      OutputOptions_LINEDELETE | OutputOptions_NOLOG);

   // wait for all packages to finish, because we cannot proceed if not all data was fetched
   // BUT : update output each OUTPUT_INTERVAL_MS ms
   while (! finishedWork.timedWaitForCount(generatedWork, MODEENABLEQUOTA_OUTPUT_INTERVAL_MS) )
   {
      FsckTkEx::fsckOutput("Processed " + StringTk::uint64ToStr(fileCount.read()) + " entries",
         OutputOptions_LINEDELETE | OutputOptions_NOLOG);
   }

   FsckTkEx::fsckOutput("Processed " + StringTk::uint64ToStr(fileCount.read()) + " entries",
      OutputOptions_LINEDELETE | OutputOptions_NOLOG | OutputOptions_LINEBREAK);

   if ( errorCount.read() > 0 )
   {
      FsckTkEx::fsckOutput("", OutputOptions_LINEBREAK);
      FsckTkEx::fsckOutput(
         StringTk::uint64ToStr(errorCount.read())
            + " errors occurred. Please see Metadata server logs for more details.",
         OutputOptions_ADDLINEBREAKBEFORE | OutputOptions_LINEBREAK);
   }
}
