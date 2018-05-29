#include <app/App.h>
#include <app/config/Config.h>
#include <common/app/log/Logger.h>
#include <common/cache/toolkit/cachepathtk.h>
#include <common/cache/toolkit/filesystemtk.h>
#include <common/cache/toolkit/threadvartk.h>
#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "DiscardWork.h"

void DiscardWork::doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   /**
    * used nftw flags for delete directories
    * FTW_ACTIONRETVAL  handles the return value from the fn(), fn() must return special values
    * FTW_PHYS          do not follow symbolic links
    * FTW_MOUNT         stay within the same file system
    * FTW_DEPTH         do a post-order traversal, call fn() for the directory itself after handling
    *                   the contents of the directory and its subdirectories. By default, each
    *                   directory is handled before its contents.
    */
   const int nftwFlagsDelete = FTW_ACTIONRETVAL | FTW_PHYS | FTW_MOUNT | FTW_DEPTH;

   bool doFollowSymlink = deeperFlags & DEEPER_FLUSH_FOLLOWSYMLINKS;

   if(doFollowSymlink)
      threadvartk::setThreadKeyNftwFollowSymlink(true);

   int retVal = nftw(sourcePath.c_str(), filesystemtk::nftwDelete, filesystemtk::NFTW_MAX_OPEN_FD,
      nftwFlagsDelete);
   if(retVal) // delete the directory only when the directory is empty
   {
      retVal = rmdir(sourcePath.c_str() );
      if(retVal)
         Logger::getLogger()->log(Log_ERR, "copyDir",
            "Could not delete directory after copy. path: " + sourcePath +
            "; errno: " + System::getErrString(errno) );
   }

   if(doFollowSymlink)
   {
      threadvartk::setThreadKeyNftwFollowSymlink(false);
      threadvartk::resetThreadKeyNftwNumFollowedSymlinks();
   }

   if(retVal != DEEPER_RETVAL_SUCCESS)
      updateResult(errno);
}
