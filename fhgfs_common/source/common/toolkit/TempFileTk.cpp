#include <common/app/log/Logger.h>
#include <common/app/AbstractApp.h>

#include <libgen.h>
#include <new>

#include "TempFileTk.h"

namespace TempFileTk {

/**
 * Stores data to a temporary file first, and then moves it over to the final filename. This
 * ensures that there's always a consistent file on disk, even if the program is aborted halfway
 * through the write process.
 * @param filename the name of the final file. If the file exists, it will be overwritten.
 * @param contents data to write to the file
 */
FhgfsOpsErr storeTmpAndMove(const std::string& filename, const std::vector<char>& contents)
{
   // Need a copy of the string, because the Xs will be replaced by mkstemp.
   std::string tmpname = filename + ".tmp-XXXXXX\0"; // terminating 0 needed for c library calls

   // Open temp file
   int fd = mkstemp(&tmpname[0]);
   if (fd == -1)
   {
      FhgfsOpsErr err = FhgfsOpsErrTk::fromSysErr(errno);
      LOG(ERR, "Could not open temporary file.", tmpname,
            as("sysErr", FhgfsOpsErrTk::toErrString(err)));
      return err;
   }

   // Write to temp file
   ssize_t written = 0;
   while (written < (ssize_t)contents.size())
   {
      ssize_t writeRes = write(fd, &contents[0] + written, contents.size() - written);
      if (writeRes == -1)
      {
         FhgfsOpsErr err = FhgfsOpsErrTk::fromSysErr(errno);
         LOG(ERR, "Could not write to temporary file", tmpname,
               as("sysErr", FhgfsOpsErrTk::toErrString(err)));
         return err;
      }

      written += writeRes;
   }

   fsync(fd);
   close(fd);

   // Move temp file over final file
   int renameRes = rename(tmpname.c_str(), filename.c_str());
   if (renameRes == -1)
   {
      FhgfsOpsErr err = FhgfsOpsErrTk::fromSysErr(errno);
      LOG(ERR, "Renaming failed.", as("from", tmpname), as("to", filename),
            as("sysErr", FhgfsOpsErrTk::toErrString(err)));
      return err;
   }


   // Sync directory
   char* dirName = dirname(&tmpname[0]); // manpage says: dirname may modify the contents
   int dirFd = open(dirName, O_DIRECTORY | O_RDONLY);
   if (dirFd == -1)
   {
      FhgfsOpsErr err = FhgfsOpsErrTk::fromSysErr(errno);
      LOG(ERR, "Could not write to temporary file", tmpname,
            as("sysErr", FhgfsOpsErrTk::toErrString(err)));
      return err;
   }

   fsync(dirFd);
   close(dirFd);

   return FhgfsOpsErr_SUCCESS;
}

};
