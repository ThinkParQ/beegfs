#ifndef MSGHELPERGENERICDEBUG_H_
#define MSGHELPERGENERICDEBUG_H_

#include <common/Common.h>


#define GENDBGMSG_OP_VARLOGMESSAGES       "varlogmessages"
#define GENDBGMSG_OP_VARLOGKERNLOG        "varlogkernlog"
#define GENDBGMSG_OP_FHGFSLOG             "fhgfslog"
#define GENDBGMSG_OP_LOADAVG              "loadavg"
#define GENDBGMSG_OP_DROPCACHES           "dropcaches"
#define GENDBGMSG_OP_GETCFG               "getcfg"
#define GENDBGMSG_OP_GETLOGLEVEL          "getloglevel"
#define GENDBGMSG_OP_SETLOGLEVEL          "setloglevel"


class MsgHelperGenericDebug
{
   public:
      static std::string processOpVarLogMessages(std::istringstream& commandStream);
      static std::string processOpVarLogKernLog(std::istringstream& commandStream);
      static std::string processOpFhgfsLog(std::istringstream& commandStream);
      static std::string processOpLoadAvg(std::istringstream& commandStream);
      static std::string processOpDropCaches(std::istringstream& commandStream);
      static std::string processOpCfgFile(std::istringstream& commandStream, std::string cfgFile);
      static std::string processOpGetLogLevel(std::istringstream& commandStream);
      static std::string processOpSetLogLevel(std::istringstream& commandStream);
      
      static std::string loadTextFile(std::string path);
      static std::string writeTextFile(std::string path, std::string writeStr);

   private:
      MsgHelperGenericDebug() {}


   public:
};

#endif /* MSGHELPERGENERICDEBUG_H_ */
