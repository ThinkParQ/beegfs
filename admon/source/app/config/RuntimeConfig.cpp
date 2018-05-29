#include "RuntimeConfig.h"

void RuntimeConfig::setDefaults()
{
   char localHost[256];

   gethostname(localHost, 256);
   // setting defaults
   mailEnabled = false;
   mailSmtpSendTypeNum = SmtpSendType_SOCKET;
   mailSendmailPath = MAIL_SENDMAIL_DEFAULT_PATH;
   mailSender = "beegfs_admon@" + std::string(localHost);
   mailRecipient = "";
   mailSmtpServer = "";
   mailMinDownTimeSec = 10;
   mailResendMailTimeMin = 60;
   scriptPath = "";
}
