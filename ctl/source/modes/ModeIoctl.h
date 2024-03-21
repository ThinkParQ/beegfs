#ifndef MODEIOCTL_H_
#define MODEIOCTL_H_

#include <common/Common.h>
#include <toolkit/IoctlTk.h>
#include "Mode.h"


class ModeIoctl : public Mode
{
   public:
      ModeIoctl()
      {
         // nothing to be done here, currently
      }

      virtual int execute();

      static void printHelp();


   private:
      int ioctlGetMountID(IoctlTk& ioctlTk);
      int ioctlGetCfgFile(IoctlTk& ioctlTk);
      int ioctlGetRuntimeCfgFile(IoctlTk& ioctlTk);
      int ioctlTestIsFhgfs(IoctlTk& ioctlTk);
      int ioctlGetStripeInfo(IoctlTk& ioctlTk);
      int ioctlGetStripeTargets(IoctlTk& ioctlTk);
      int ioctlMkFileWithStripeHints();
      int ioctlGetInodeID();
      int ioctlGetEntryInfo(IoctlTk& ioctlTk);
      int ioctlPingNode();

};

#endif /* MODEIOCTL_H_ */
