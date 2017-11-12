#include <app/App.h>
#include <program/Program.h>
#include "ModeHelp.h"


#define MODEHELP_ARG_SPECIFICHELP   RUNMODE_HELP_KEY_STRING /* print help for a specific mode */
#define MODEHELP_ARG_HIDDEN         "--hidden"

int ModeHelp::execute()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   if (printAllModes)
   {
      printSpecificHelpHeader();

      for (auto* mode = &__RunModes[0]; mode->modeString != nullptr; mode++)
      {
         if (mode->hidden && !cfg->getUnknownConfigArgs()->count(MODEHELP_ARG_HIDDEN))
            continue;
         if (!mode->printHelp)
            continue;

         std::cout << std::endl;
         std::cout << "    ================================================    " << std::endl;
         std::cout << std::endl;
         std::cout << "                MODE " << mode->modeString << std::endl;
         std::cout << std::endl;
         mode->printHelp();
      }
   }
   else
   {
      const RunModesElem* runMode = cfg->determineRunMode();

      if (!runMode)
         printGeneralHelp();
      else
         printSpecificHelp(*runMode);
   }

   return APPCODE_NO_ERROR;
}

void ModeHelp::printGeneralHelp()
{
   std::cout << "BeeGFS Command-Line Control Tool (http://www.beegfs.com)" << std::endl;
   std::cout << "Version: " << BEEGFS_VERSION << std::endl;
   std::cout << std::endl;
   std::cout << "GENERAL USAGE:" << std::endl;
   std::cout << " $ beegfs-ctl --<modename> --help" << std::endl;
   std::cout << " $ beegfs-ctl --<modename> [mode_arguments] [client_arguments]" << std::endl;
   std::cout << std::endl;
   std::cout << "MODES:" << std::endl;
   std::cout << " --listnodes             => List registered clients and servers." << std::endl;
   std::cout << " --listtargets           => List metadata and storage targets." << std::endl;
   std::cout << " --removenode (*)        => Remove (unregister) a node." << std::endl;
   std::cout << " --removetarget (*)      => Remove (unregister) a storage target." << std::endl;
   std::cout << std::endl;
   std::cout << " --getentryinfo          => Show file system entry details." << std::endl;
   std::cout << " --setpattern (*)        => Set a new striping configuration." << std::endl;
   std::cout << " --mirrormd (*)          => Enable metadata mirroring." << std::endl;
   //std::cout << " --reverselookup       => Resolve path for file or directory ID. (EXPERIMENTAL)" << std::endl;
   std::cout << " --find                  => Find files located on certain servers." << std::endl;
   std::cout << " --refreshentryinfo      => Refresh file system entry metadata." << std::endl;
   std::cout << std::endl;
   std::cout << " --createfile (*)        => Create a new file." << std::endl;
   std::cout << " --createdir (*)         => Create a new directory." << std::endl;
   std::cout << " --migrate               => Migrate files to other storage servers." << std::endl;
   std::cout << " --disposeunused (*)     => Purge remains of unlinked files." << std::endl;
   std::cout << std::endl;
   std::cout << " --serverstats           => Show server IO statistics." << std::endl;
   std::cout << " --clientstats           => Show client IO statistics." << std::endl;
   std::cout << " --userstats             => Show user IO statistics." << std::endl;
   std::cout << " --storagebench (*)      => Run a storage targets benchmark." << std::endl;
   std::cout << std::endl;
   std::cout << " --getquota              => Show quota information for users or groups." << std::endl;
   std::cout << " --setquota (*)          => Sets the quota limits for users or groups." << std::endl;
   std::cout << std::endl;
   std::cout << " --listmirrorgroups      => List mirror buddy groups." << std::endl;
   std::cout << " --addmirrorgroup (*)    => Add a mirror buddy group." << std::endl;
   std::cout << " --startresync (*)       => Start resync of a storage target or metadata node." << std::endl;
   std::cout << " --resyncstats           => Get statistics on a resync." << std::endl;
   std::cout << " --setstate (*)          => Manually set the consistency state of a target or" << std::endl;
   std::cout << "                            metadata node." << std::endl;
   std::cout << std::endl;
   std::cout << " --liststoragepools      => List storage pools." << std::endl;
   std::cout << " --addstoragepool (*)    => Add a storage pool." << std::endl;
   std::cout << " --removestoragepool (*) => Remove a storage pool." << std::endl;
   std::cout << " --modifystoragepool (*) => Modify a storage pool." << std::endl;
   std::cout << std::endl;
   std::cout << "*) Marked modes require root privileges." << std::endl;
   std::cout << std::endl;
   std::cout << "USAGE:" << std::endl;
   std::cout << " This is the BeeGFS command-line control tool." << std::endl;
   std::cout << std::endl;
   std::cout << " Choose a control mode from the list above and use the parameter \"--help\" to" << std::endl;
   std::cout << " show arguments and usage examples for that particular mode." << std::endl;
   std::cout << std::endl;
   std::cout << " Example: Show help for mode \"--listnodes\"" << std::endl;
   std::cout << "  $ beegfs-ctl --listnodes --help" << std::endl;
}

/**
 * Note: don't call this with RunMode_INVALID.
 */
void ModeHelp::printSpecificHelp(const RunModesElem& runMode)
{
   printSpecificHelpHeader(); // print general usage and client options info
   std::cout << std::endl;
   runMode.printHelp();
}

/**
 * Print the help message header that applies to any specific mode help. Contains general usage
 * and client options info.
 */
void ModeHelp::printSpecificHelpHeader()
{
   std::cout << "GENERAL USAGE:" << std::endl;
   std::cout << "  $ beegfs-ctl --<modename> [mode_arguments] [client_arguments]" << std::endl;
   std::cout << std::endl;
   std::cout << "CLIENT ARGUMENTS:" << std::endl;
   std::cout << " Optional:" << std::endl;
   std::cout << "  --mount=<path>           Use config settings from this BeeGFS mountpoint" << std::endl;
   std::cout << "                           (instead of \"--cfgFile\")." << std::endl;
   std::cout << "  --cfgFile=<path>         Path to BeeGFS client config file." << std::endl;
   std::cout << "                           (Default: " CONFIG_DEFAULT_CFGFILENAME ")" << std::endl;
   std::cout << "  --logEnabled             Enable detailed logging." << std::endl;
   std::cout << "  --<key>=<value>          Any setting from the client config file to override" << std::endl;
   std::cout << "                           the config file values (e.g. \"--logLevel=5\")." << std::endl;
}

