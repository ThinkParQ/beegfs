#ifndef MODEENABLEQUOTA_H
#define MODEENABLEQUOTA_H

#include <app/config/Config.h>
#include <database/FsckDB.h>
#include <modes/Mode.h>

#define MODEENABLEQUOTA_OUTPUT_INTERVAL_MS 5000

class ModeEnableQuota : public Mode
{
   public:
      ModeEnableQuota();

      static void printHelp();

      virtual int execute();

   private:
      LogContext log;

      void printHeaderInformation();

      void fixPermissions();
};

#endif /* MODEENABLEQUOTA_H */
