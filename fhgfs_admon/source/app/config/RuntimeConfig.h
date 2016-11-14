#ifndef RUNTIMECONFIG_H_
#define RUNTIMECONFIG_H_

#include <common/toolkit/StringTk.h>
#include <common/Common.h>
#include "Config.h"


class RuntimeConfig
{
   private:
      Config *cfg;
      bool mailEnabled;
      std::string mailSender;
      std::string mailRecipient;
      std::string mailSmtpServer;
      int mailMinDownTimeSec;
      int mailResendMailTimeMin;
      std::string scriptPath;

      void setDefaults();
   public:
      RuntimeConfig(Config *cfg)
      {
         this->cfg = cfg;
         setDefaults();
      }

      bool getMailEnabled()
      {
         if (cfg->isMailConfigOverrideActive())
         {
            return cfg->getMailEnabled();
         }
         return mailEnabled;
      }

      void setMailEnabled(bool mailEnabled)
      {
         this->mailEnabled = mailEnabled;
      }

      std::string getMailSender()
      {
         if (cfg->isMailConfigOverrideActive())
         {
            return cfg->getMailSender();
         }
         return mailSender;
      }

      void setMailSender(std::string mailSender)
      {
         this->mailSender = mailSender;
      }

      std::string getMailRecipient()
      {
         if (cfg->isMailConfigOverrideActive())
         {
            return cfg->getMailRecipient();
         }
         return mailRecipient;
      }

      void setMailRecipient(std::string mailRecipient)
      {
         this->mailRecipient = mailRecipient;
      }

      std::string getMailSmtpServer()
      {
         if (cfg->isMailConfigOverrideActive())
         {
            return cfg->getMailSmtpServer();
         }
         return mailSmtpServer;
      }

      void setMailSmtpServer(std::string mailSmtpServer)
      {
         this->mailSmtpServer = mailSmtpServer;
      }

      int getMailMinDownTimeSec()
      {
         if (cfg->isMailConfigOverrideActive())
         {
            return cfg->getMailMinDownTimeSec();
         }
         return mailMinDownTimeSec;
      }

      void setMailMinDownTimeSec(int mailMinDownTimeSec)
      {
         this->mailMinDownTimeSec = mailMinDownTimeSec;
      }

      int getMailResendMailTimeMin()
      {
         if (cfg->isMailConfigOverrideActive())
         {
            return cfg->getMailResendMailTimeMin();
         }
         return mailResendMailTimeMin;
      }

      void setMailResendMailTimeMin(int mailResendMailTimeMin)
      {
         this->mailResendMailTimeMin = mailResendMailTimeMin;
      }

      void setScriptPath(std::string scriptPath)
      {
         this->scriptPath = scriptPath;
      }

      std::string getScriptPath()
      {
         return this->scriptPath;
      }

      void exportAsMap(std::map<std::string, std::string> *outMap)
      {
         if(mailEnabled)
         {
            (*outMap)["mailEnabled"] = "true";
         }
         else
         {
            (*outMap)["mailEnabled"] = "false";
         }
         (*outMap)["mailSender"] = mailSender;
         (*outMap)["mailRecipient"] = mailRecipient;
         (*outMap)["mailSmtpServer"] = mailSmtpServer;
         (*outMap)["mailMinDownTimeSec"] = StringTk::intToStr(mailMinDownTimeSec);
         (*outMap)["mailResendMailTimeMin"] = StringTk::intToStr(mailResendMailTimeMin);
         (*outMap)["scriptPath"] = scriptPath;

      }

      void importAsMap(std::map<std::string, std::string> *inMap)
      {
         mailEnabled = StringTk::strToBool((*inMap)["mailEnabled"]);
         mailSender = (*inMap)["mailSender"];
         mailRecipient = (*inMap)["mailRecipient"];
         mailSmtpServer = (*inMap)["mailSmtpServer"];
         mailMinDownTimeSec = StringTk::strToInt((*inMap)["mailMinDownTimeSec"]);
         mailResendMailTimeMin = StringTk::strToInt((*inMap)["mailResendMailTimeMin"]);
         scriptPath = (*inMap)["scriptPath"];
      }

      virtual ~RuntimeConfig()
      {
      }
};

#endif /* RUNTIMECONFIG_H_ */
