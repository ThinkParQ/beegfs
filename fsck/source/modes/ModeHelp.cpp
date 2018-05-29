#include "ModeHelp.h"

#include <program/Program.h>

#include <modes/ModeCheckFS.h>
#include <modes/ModeEnableQuota.h>

int ModeHelp::execute()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   RunMode runMode = cfg->determineRunMode();

   if(runMode == RunMode_INVALID)
      printGeneralHelp();
   else
      printSpecificHelp(runMode);

   std::cout << std::endl;

   return APPCODE_NO_ERROR;
}

void ModeHelp::printGeneralHelp()
{
   std::cout << "BeeGFS File System Check (http://www.beegfs.com)" << std::endl;
   std::cout << "Version: " << BEEGFS_VERSION << std::endl;
   std::cout << std::endl;
   std::cout << "GENERAL USAGE:" << std::endl;
   std::cout << " $ beegfs-fsck --<modename> --help" << std::endl;
   std::cout << " $ beegfs-fsck --<modename> [mode_arguments] [client_arguments]" << std::endl;
   std::cout << std::endl;
   std::cout << "MODES:" << std::endl;
   std::cout << " --checkfs     => Perform a full check and optional repair of " << std::endl;
   std::cout << "                  a BeeGFS instance." << std::endl;
   std::cout << " --enablequota => Set attributes needed for quota support in FhGFS." << std::endl;
   std::cout << "                  Can be used to enable quota support on an existing" << std::endl;
   std::cout << "                  system." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This is the BeeGFS file system consistency check and repair tool." << std::endl;
   std::cout << std::endl;
   std::cout << " Choose a mode from the list above and use the parameter \"--help\"" << std::endl;
   std::cout << " to show arguments and usage examples for that particular mode." << std::endl;
   std::cout << std::endl;
   std::cout << " (Running beegfs-fsck requires root privileges.)" << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show help for mode \"--checkfs\"" << std::endl;
   std::cout << "  $ beegfs-fsck --checkfs --help" << std::endl;
}


/**
 * Note: don't call this with RunMode_INVALID.
 */
void ModeHelp::printSpecificHelp(RunMode runMode)
{
   printSpecificHelpHeader(); // print general usage and client options info

   switch(runMode)
   {
      case RunMode_CHECKFS:
      {
         ModeCheckFS::printHelp();
      } break;

      case RunMode_ENABLEQUOTA:
      {
         ModeEnableQuota::printHelp();
      } break;

      default:
      {
         std::cerr << "Error: Unhandled mode specified. Mode number: " << runMode << std::endl;
         std::cout << std::endl;

         printGeneralHelp();
      } break;
   }

}

/**
 * Print the help message header that applies to any specific mode help. Contains general usage
 * and client options info.
 */
void ModeHelp::printSpecificHelpHeader()
{
   std::cout << "GENERAL USAGE:" << std::endl;
   std::cout << "  $ beegfs-fsck --<modename> [mode_arguments] [client_arguments]" << std::endl;
   std::cout << std::endl;
   std::cout << "CLIENT ARGUMENTS:" << std::endl;
   std::cout << " --cfgFile=<path>         Path to BeeGFS client config file." << std::endl;
   std::cout << "                          (Default: " CONFIG_DEFAULT_CFGFILENAME ")" << std::endl;
   std::cout << " --<key>=<value>          Any setting from the client config file to override" << std::endl;
   std::cout << "                          the config file values (e.g. \"--logLevel=5\")." << std::endl;
   std::cout << std::endl;
}
