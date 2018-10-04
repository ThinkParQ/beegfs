#ifndef MODEREMOVEDIRENTRY_H_
#define MODEREMOVEDIRENTRY_H_

#include <common/Common.h>
#include "Mode.h"


class ModeRemoveDirEntry : public Mode
{
   public:
      ModeRemoveDirEntry()
      {
         cfgReadFromStdin = false;
      }

      virtual int execute();

      static void printHelp();


   private:
      bool cfgReadFromStdin;

      bool removeDirEntry(Node& ownerNode, EntryInfo* parentInfo, std::string entryName);
};

#endif /* MODEREMOVEDIRENTRY_H_ */
