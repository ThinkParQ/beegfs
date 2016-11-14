#ifndef MODEGETENTRYINFO_H_
#define MODEGETENTRYINFO_H_

#include <common/Common.h>
#include <common/storage/striping/StripePattern.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/PathInfo.h>

#include "Mode.h"

class ModeGetEntryInfo : public Mode
{
   public:
      ModeGetEntryInfo()
      {
         this->cfgReadFromStdin = false;
         this->cfgUseMountedPath = true;
         this->cfgVerbose = false;
         this->cfgShowTargetMappings = true;
      }

      virtual int execute();

      static void printHelp();


   private:
      std::string cfgPathStr;
      bool cfgUseMountedPath; // false if user-given path is relative to FhGFS mountpoint
      bool cfgReadFromStdin;
      bool cfgVerbose;
      bool cfgShowTargetMappings; // show the servers to which stripe targets are mapped

      bool getAndPrintEntryInfo(Node& ownerNode, EntryInfo* entryInfo, PathInfo* outPathInfo);
      void printPattern(StripePattern* pattern);
      void printTargetMapping(uint16_t targetID);
};

#endif /*MODEGETENTRYINFO_H_*/
