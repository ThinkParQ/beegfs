#include <common/fsck/FsckChunk.h>
#include <common/toolkit/Time.h>
#include <net/msghelpers/MsgHelperIO.h>
#include <program/Program.h>
#include "StorageTkEx.h"

#include <libgen.h>

bool StorageTkEx::attachEntryInfoToChunk(int fd, const char *serialEntryInfoBuf,
   unsigned serialEntryInfoBufLen, bool overwriteExisting)
{
   const char* logContext = "StorageTkEx (store backlink xattr)";

   int flags = 0;
   if (!overwriteExisting)
      flags = XATTR_CREATE;

   // set owner node id
   int setRes = fsetxattr(fd, CHUNK_XATTR_ENTRYINFO_NAME, serialEntryInfoBuf, serialEntryInfoBufLen,
      flags);

   // if errno is EEXIST, that's because XATTR_CREATE was set and is not an error
   if ( (setRes == -1 ) && (errno != EEXIST) )
   { // error
      if(errno == ENOTSUP)
         LogContext(logContext).logErr("Unable to store chunk xattr for entry info. Did you "
            "enable extended attributes (user_xattr) on the underlying file system?");
      else
         LogContext(logContext).logErr("Unable to store chunk xattr for"
            " entry info. SysErr: " +
            System::getErrString() );

      return false;
   }

   return true;
}

bool StorageTkEx::attachEntryInfoToChunk(int targetFD, PathInfo* pathInfo, std::string chunkID,
   const char *serialEntryInfoBuf, unsigned serialEntryInfoBufLen, bool overwriteExisting)
{
   const char* logContext = "StorageTkEx (store backlink xattr)";

   std::string filepath = StorageTk::getFileChunkPath(pathInfo, chunkID);

   int flags = 0;
   if (!overwriteExisting)
      flags = XATTR_CREATE;

   // set owner node id
   int setRes = MsgHelperIO::setXattrAt(targetFD, filepath.c_str(), CHUNK_XATTR_ENTRYINFO_NAME,
      serialEntryInfoBuf, serialEntryInfoBufLen, flags);

   // if errno is EEXIST, that's because XATTR_CREATE was set and is not an error
   if ( (setRes == -1 ) && (errno != EEXIST) )
   { // error
      if(errno == ENOENT) // chunk does not exist => this is not considered to be an error!
      {
         LogContext(logContext).log(Log_SPAM, "Unable to store chunk xattr. "
            "Chunk with ID " + chunkID + " does not exist on given target.");
         return true;
      }
      else
      if(errno == ENOTSUP)
         LogContext(logContext).logErr("Unable to store chunk xattr for entry info. Did you "
            "enable extended attributes (user_xattr) on the underlying file system?");
      else
         LogContext(logContext).logErr("Unable to store chunk xattr for entry info. "
            "Chunk ID: " + chunkID + "; "
            "SysErr: " + System::getErrString() );

      return false;
   }

   return true;
}

bool StorageTkEx::readEntryInfoFromChunk(uint16_t targetID, PathInfo* pathInfo, std::string chunkID,
   EntryInfo* outEntryInfo)
{
   const char* logContext = "StorageTkEx (store backlink xattr)";

   int targetFD = Program::getApp()->getTargetFD(targetID, false);

   if(unlikely(targetFD == -1))
   { // unknown targetID
      LogContext (logContext).logErr(std::string("Unknown targetID: ") +
         StringTk::uintToStr(targetID));

      return false;
   }

   std::string filepath = StorageTk::getFileChunkPath(pathInfo, chunkID);

   boost::scoped_array<char> buf(new (std::nothrow) char[MAX_SERIAL_ENTRYINFO_SIZE]);
   ssize_t getRes = MsgHelperIO::getXattrAt(targetFD, filepath.c_str(), CHUNK_XATTR_ENTRYINFO_NAME,
      buf.get(), MAX_SERIAL_ENTRYINFO_SIZE);

   if (getRes > 0)
   {
      Deserializer des(buf.get(), MAX_SERIAL_ENTRYINFO_SIZE);
      des % *outEntryInfo;
      return des.good();
   }
   else
   {
      LogContext(logContext).log(Log_DEBUG, "Unable to read chunk xattr for "
         "chunk with ID " + chunkID + " "
         "on target " + StringTk::uintToStr(targetID) + "; "
         "file path: " + filepath);
      return false;
   }
}

