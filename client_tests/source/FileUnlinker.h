#ifndef source_FileUnlinker_h_dB2AV5CFF3ywY1WAe2S5gf
#define source_FileUnlinker_h_dB2AV5CFF3ywY1WAe2S5gf

#include <unistd.h>
#include <string>
#include "SafeFD.h"

class FileUnlinker {
   private:
      SafeFD dir;
      std::string fileName;

   public:
      FileUnlinker(SafeFD dir, std::string fileName)
         : dir(std::move(dir) ), fileName(fileName)
      {
      }

      FileUnlinker()
         : dir(-1)
      {
      }

      FileUnlinker(FileUnlinker&&) = default;
      FileUnlinker& operator=(FileUnlinker&&) = default;

      ~FileUnlinker()
      {
         if(std::uncaught_exception() )
            return;

         if(dir.raw() >= 0)
            ::unlinkat(this->dir.raw(), this->fileName.c_str(), 0);
      }
};

#endif
