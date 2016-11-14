#include "RuntimeConfig.h"

void RuntimeConfig::setDefaults()
{
   char localHost[256];

   gethostname(localHost, 256);
   // setting defaults
   mailEnabled = false;
   mailSender = "beegfs_admon@" + std::string(localHost);
   mailRecipient = "";
   mailSmtpServer = "";
   mailMinDownTimeSec = 10;
   mailResendMailTimeMin = 60;
   scriptPath = "";
}
