#ifndef SESSIONTK_H_
#define SESSIONTK_H_

#include <common/Common.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/StringTk.h>


class SessionTk
{
   public:

   private:
      SessionTk() {}

   public:
      // inliners

      /**
       * note: fileHandleID format: <ownerFDHex>#<fileID>
       * note2: not supposed to be be on meta servers, use EntryInfo there
       */
      static std::string fileIDFromHandleID(std::string fileHandleID)
      {
         std::string::size_type divPos = fileHandleID.find_first_of("#", 0);

         if(unlikely(divPos == std::string::npos) )
         { // this case should never happen
            return fileHandleID;
         }

         return fileHandleID.substr(divPos + 1);
      }

      /**
       * note: fileHandleID format: <ownerFDHex>#<fileID>
       */
      static unsigned ownerFDFromHandleID(std::string fileHandleID)
      {
         std::string::size_type divPos = fileHandleID.find_first_of("#", 0);

         if(unlikely(divPos == std::string::npos) )
         { // this case should never happen
            return 0;
         }

         std::string ownerFDStr = fileHandleID.substr(0, divPos);

         return StringTk::strHexToUInt(ownerFDStr);
      }

      /**
       * note: fileHandleID format: <ownerFDHex>#<fileID>
       */
      static std::string generateFileHandleID(unsigned ownerFD, std::string fileID)
      {
         return StringTk::uintToHexStr(ownerFD) + '#' + fileID;
      }

      static int sysOpenFlagsFromFhgfsAccessFlags(unsigned accessFlags)
      {
         int openFlags = O_LARGEFILE;

         if(accessFlags & OPENFILE_ACCESS_READWRITE)
            openFlags |= O_RDWR;
         else
         if(accessFlags & OPENFILE_ACCESS_WRITE)
            openFlags |= O_WRONLY;
         else
            openFlags |= O_RDONLY;

         if(accessFlags & OPENFILE_ACCESS_DIRECT)
            openFlags |= O_DIRECT;

         if(accessFlags & OPENFILE_ACCESS_SYNC)
            openFlags |= O_SYNC;

         return openFlags;
      }
};


#endif /* SESSIONTK_H_ */
