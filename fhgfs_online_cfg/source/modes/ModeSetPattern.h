#ifndef MODESETPATTERN_H_
#define MODESETPATTERN_H_

#include <common/Common.h>
#include "Mode.h"


class ModeSetPattern : public Mode
{
   public:
      ModeSetPattern()
      {
         cfgUseRaid10Pattern = false;
         cfgUseBuddyMirrorPattern = false;
         cfgUseMountedPath = true;
         cfgReadFromStdin = false;
         actorUID = 0;
      }
      
      virtual int execute();
   
      static void printHelp();

      
   private:
      std::string cfgPathStr;
      bool cfgUseRaid10Pattern; // true to use RAID10 pattern instead of default RAID0
      bool cfgUseBuddyMirrorPattern; // true to use buddy mirror pattern instead of default RAID0
      bool cfgUseMountedPath; // false if user-given path is relative to FhGFS mountpoint
      bool cfgReadFromStdin; // true if paths should be read from stdin
      uint32_t actorUID;

      bool setPattern(Node& ownerNode, EntryInfo* entryInfo, unsigned chunkSize,
         unsigned numTargets);
   
};

#endif /*MODESETPATTERN_H_*/
