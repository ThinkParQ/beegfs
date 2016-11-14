#ifndef METADATATK_H_
#define METADATATK_H_

#include "Common.h"
#include "StorageDefinitions.h"


class MetadataTk
{
   public:

   private:
      MetadataTk() {}


   public:
      // inliners
      static DirEntryType posixFileTypeToDirEntryType(unsigned posixFileMode)
      {
         if(S_ISDIR(posixFileMode) )
            return DirEntryType_DIRECTORY;

         if(S_ISREG(posixFileMode) )
            return DirEntryType_REGULARFILE;

         if(S_ISLNK(posixFileMode) )
            return DirEntryType_SYMLINK;

         if(S_ISBLK(posixFileMode) )
            return DirEntryType_BLOCKDEV;

         if(S_ISCHR(posixFileMode) )
            return DirEntryType_CHARDEV;

         if(S_ISFIFO(posixFileMode) )
            return DirEntryType_FIFO;

         if(S_ISSOCK(posixFileMode) )
            return DirEntryType_SOCKET;

         return DirEntryType_INVALID;
      }
};



#endif /*METADATATK_H_*/
