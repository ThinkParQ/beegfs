#include <common/app/AbstractApp.h>
#include <common/system/System.h>
#include <common/threading/PThread.h>
#include "MsgHelperGenericDebug.h"


#define GENDBGMSG_TXTFILE_MAX_READ_LEN    (100*1024)


std::string MsgHelperGenericDebug::processOpVarLogMessages(std::istringstream& commandStream)
{
   return loadTextFile("/var/log/messages");
}

std::string MsgHelperGenericDebug::processOpVarLogKernLog(std::istringstream& commandStream)
{
   return loadTextFile("/var/log/kern.log");
}

std::string MsgHelperGenericDebug::processOpFhgfsLog(std::istringstream& commandStream)
{
   ICommonConfig* cfg = PThread::getCurrentThreadApp()->getCommonConfig();

   if(cfg->getLogStdFile().empty() )
      return "No log file defined.";

   return loadTextFile(cfg->getLogStdFile() );
}

std::string MsgHelperGenericDebug::processOpLoadAvg(std::istringstream& commandStream)
{
   return loadTextFile("/proc/loadavg");
}

std::string MsgHelperGenericDebug::processOpDropCaches(std::istringstream& commandStream)
{
   return writeTextFile("/proc/sys/vm/drop_caches", "3");
}

std::string MsgHelperGenericDebug::processOpGetLogLevel(std::istringstream& commandStream)
{
   // procotol: no arguments

   Logger* log = PThread::getCurrentThreadApp()->getLogger();
   std::ostringstream responseStream;

   responseStream << "Level: " << log->getLogLevel();

   return responseStream.str();
}

std::string MsgHelperGenericDebug::processOpSetLogLevel(std::istringstream& commandStream)
{
   // procotol: new log level as only argument

   Logger* log = PThread::getCurrentThreadApp()->getLogger();
   std::ostringstream responseStream;

   std::string logLevelStr;

   // get logLevel from command string
   std::getline(commandStream, logLevelStr, ' ');

   if(logLevelStr.empty() )
      return "Invalid or missing logLevel";

   int logLevel = StringTk::strToInt(logLevelStr);

   responseStream << "Old level: " << log->getLogLevel() << std::endl;

   log->setLogLevel(logLevel);

   responseStream << "New level: " << log->getLogLevel();

   return responseStream.str();
}

/**
 * @param cfgFile may be an empty string, in which case an error message will be returned.
 */
std::string MsgHelperGenericDebug::processOpCfgFile(std::istringstream& commandStream,
   std::string cfgFile)
{
   if(cfgFile.empty() )
      return "No config file defined.";

   return loadTextFile(cfgFile);
}

/**
 * Opens a text file and reads it. Only the last part (see GENDBGMSG_TXTFILE_MAX_READ_LEN) is
 * returned if the file is too large.
 *
 * @return file contents or human-readable error string will be returned if file cannot be read.
 */
std::string MsgHelperGenericDebug::loadTextFile(std::string path)
{
   const size_t maxLineLen = 2048;
   char line[maxLineLen];
   std::ostringstream responseStream;
   size_t bytesRead = 0;

   struct stat statBuf;

   std::ifstream fis(path.c_str() );
   if(!fis.is_open() || fis.fail() )
      return "Unable to open file: " + path + ". (SysErr: " + System::getErrString() + ")";

   // seek if file too large

   int statRes = stat(path.c_str(), &statBuf);
   if(statRes != 0)
      return "Unable to stat file. SysErr: " + System::getErrString();


   if(statBuf.st_size > GENDBGMSG_TXTFILE_MAX_READ_LEN)
      fis.seekg(statBuf.st_size - GENDBGMSG_TXTFILE_MAX_READ_LEN);


   while(!fis.eof() )
   {
      line[0] = 0; // make sure line is zero-terminated in case of read error

      fis.getline(line, maxLineLen);

      if(!fis.eof() && fis.fail() )
      {
         responseStream << "File read error occurred. SysErr: " + System::getErrString();
         break;
      }

      // the file might grow while we're reading it, so we check the amount of read data again below
      size_t lineLen = strlen(line);
      if( (lineLen + bytesRead + 1) > GENDBGMSG_TXTFILE_MAX_READ_LEN) // (+1 for newline)
         break;

      bytesRead += lineLen + 1; // (+1 for newline)

      responseStream << line << std::endl;
   }

   fis.close();

   return responseStream.str();
}


/**
 * Opens a file and writes text to it. Will try to create the file if it doesn't exist yet. File
 * will be truncated if it exists already.
 *
 * @param writeStr: The string that should be written to the file
 * @return: human-readable error string will be returned if file cannot be read.
 */
std::string MsgHelperGenericDebug::writeTextFile(std::string path, std::string writeStr)
{
   std::ofstream fos(path.c_str() );
   if(!fos.is_open() || fos.fail() )
      return "Unable to open file. SysErr: " + System::getErrString();

   fos << writeStr;

   if(fos.fail() )
      return "Error during file write. SysErr: " + System::getErrString();

   fos.close();

   // (note: errno doesn't work after close, so we cannot write the corresponding errString here.)

   if(fos.fail() )
      return "Error occurred during file close.";

   return "Completed successfully";
}
