#ifndef MODEREFRESHENTRYINFO_H_
#define MODEREFRESHENTRYINFO_H_

#include <common/Common.h>
#include <common/storage/EntryInfo.h>
#include "Mode.h"


class ModeRefreshEntryInfo : public Mode
{
   public:
      ModeRefreshEntryInfo()
      {
         cfgReadFromStdin = false;
      }

      virtual int execute();

      static void printHelp();


   private:
      bool cfgReadFromStdin;

      bool refreshEntryInfo(Node& ownerNode, EntryInfo* entryInfo);

};

#endif /* MODEREFRESHENTRYINFO_H_ */
