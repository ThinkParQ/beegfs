#include <toolkit/asyncfilesystemtk.h>
#include <deeper/deeper_cache.h>
#include "ProcessDirWork.h"



void ProcessDirWork::doProcess(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   int retVal;

   retVal = asyncfilesystemtk::handleSubdirectoryFlag(sourcePath.c_str(), destPath.c_str(),
      deeperFlags, workType, rootWork ? rootWork : this, rootWork ? false : true,
      numFollowedSymlinks);

   if(retVal != DEEPER_RETVAL_SUCCESS)
      updateResult(errno);
}
