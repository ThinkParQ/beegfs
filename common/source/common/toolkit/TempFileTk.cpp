#include <common/app/log/Logger.h>
#include <common/toolkit/FDHandle.h>

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
 * @param size size of contents (in bytes)
 */
FhgfsOpsErr storeTmpAndMove(const std::string& filename, const void* contents, size_t size)
{
   // Need a copy of the string, because the Xs will be replaced by mkstemp.
   std::string tmpname = filename + ".tmp-XXXXXX" + '\0'; // terminating 0 needed for c library calls

   LOG(GENERAL, DEBUG, "Storing file using temporary file.", filename, tmpname);

   // Open temp file
   FDHandle fd(mkstemp(&tmpname[0]));
   if (!fd.valid())
   {
      auto e = errno;
      LOG(GENERAL, ERR, "Could not open temporary file.", tmpname, sysErr);
      return FhgfsOpsErrTk::fromSysErr(e);
   }

   {
      struct DeleteOnExit {
         const char* file;

         ~DeleteOnExit() {
            if (!file)
               return;

            if (unlink(file) == 0)
               return;

            LOG(GENERAL, ERR, "Failed to unlink tmpfile after error.", ("tmpname", file), sysErr);
         }
      } deleteOnExit = { tmpname.c_str() };

      // Write to temp file
      size_t written = 0;
      while (written < size)
      {
         ssize_t writeRes = write(*fd, ((const char*) contents) + written, size - written);
         if (writeRes == -1)
         {
            auto e = errno;
            LOG(GENERAL, ERR, "Could not write to temporary file", tmpname, sysErr);
            return FhgfsOpsErrTk::fromSysErr(e);
         }

         written += writeRes;
      }

      if (fsync(*fd) < 0)
      {
         LOG(GENERAL, ERR, "Could not write to temporary file", tmpname, sysErr);
         return FhgfsOpsErr_INTERNAL;
      }

      // Move temp file over final file
      int renameRes = rename(tmpname.c_str(), filename.c_str());
      if (renameRes == -1)
      {
         auto e = errno;
         LOG(GENERAL, ERR, "Renaming failed.", ("from", tmpname), ("to", filename), sysErr);
         return FhgfsOpsErrTk::fromSysErr(e);
      }

      deleteOnExit.file = nullptr;
   }

   // Sync directory
   char* dirName = dirname(&tmpname[0]); // manpage says: dirname may modify the contents
   FDHandle dirFd(open(dirName, O_DIRECTORY | O_RDONLY));
   if (!dirFd.valid())
   {
      auto e = errno;
      LOG(GENERAL, ERR, "Could not write to temporary file", tmpname, sysErr);
      return FhgfsOpsErrTk::fromSysErr(e);
   }

   if (fsync(*dirFd) < 0)
   {
      LOG(GENERAL, WARNING, "fsync of directory failed.", tmpname, sysErr);
      // do not return an error here. the alternative would be to try and restore the old state,
      // but that too might fail. it's easier and more reliable to just hope for the best now.
      return FhgfsOpsErr_SUCCESS;
   }

   return FhgfsOpsErr_SUCCESS;
}

/**
 * Stores data to a temporary file first, and then moves it over to the final filename. This
 * ensures that there's always a consistent file on disk, even if the program is aborted halfway
 * through the write process.
 * @param filename the name of the final file. If the file exists, it will be overwritten.
 * @param contents data to write to the file
 */
FhgfsOpsErr storeTmpAndMove(const std::string& filename, const std::vector<char>& contents)
{
   return storeTmpAndMove(filename, &contents[0], contents.size());
}

/**
 * Stores data to a temporary file first, and then moves it over to the final filename. This
 * ensures that there's always a consistent file on disk, even if the program is aborted halfway
 * through the write process.
 * @param filename the name of the final file. If the file exists, it will be overwritten.
 * @param contents data to write to the file
 */
FhgfsOpsErr storeTmpAndMove(const std::string& filename, const std::string& contents)
{
   return storeTmpAndMove(filename, &contents[0], contents.size());
}

};
