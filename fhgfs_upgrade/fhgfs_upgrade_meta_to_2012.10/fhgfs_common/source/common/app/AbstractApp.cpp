#include <common/toolkit/StorageTk.h>
#include "AbstractApp.h"

/**
 * Note: Will do nothing if pidFile string is empty.
 *
 * @pidFile will not be accepted if path is not absolute (=> throws exception).
 * @return file descriptor for locked pid file or -1 iff pidFile string was empty.
 */
int AbstractApp::createAndLockPIDFile(std::string pidFile) throw(InvalidConfigException)
{
   if(pidFile.empty() )
      return -1;

   // PID file defined

   Path pidPath(pidFile);
   if(!pidPath.isAbsolute() ) /* (check to avoid problems after chdir) */
      throw InvalidConfigException("Path to PID file must be absolute: " + pidFile);

   int pidFileLockFD = StorageTk::createAndLockPIDFile(pidFile, false);
   if(pidFileLockFD == -1)
      throw InvalidConfigException("Unable to create/lock PID file: " + pidFile);

   return pidFileLockFD;
}

/**
 * Updates a locked pid file, which is typically required after calling daemon().
 *
 * Note: Make sure the file is already locked by calling createAndLockPIDFile().
 * Note: Will do nothing if pidfileFD is -1.
 */
void AbstractApp::updateLockedPIDFile(int pidFileFD) throw(InvalidConfigException)
{
   if(pidFileFD == -1)
      return;

   bool updateRes = StorageTk::updateLockedPIDFile(pidFileFD);
   if(!updateRes)
      throw InvalidConfigException("Unable to update PID file");
}

/**
 * Note: Will do nothing if pidFileFD is -1.
 */
void AbstractApp::unlockPIDFile(int pidFileFD)
{
   if(pidFileFD == -1)
      return;

   StorageTk::unlockPIDFile(pidFileFD);
}


/**
 * @param component the thread that we're waiting for via join(); may be NULL (in which case this
 * method returns immediately)
 */
void AbstractApp::waitForComponentTermination(PThread* component)
{
   const char* logContext = "App (wait for component termination)";
   Logger* logger = this->getLogger();

   const int timeoutMS = 2000;

   if(!component)
      return;

   bool isTerminated = component->timedjoin(timeoutMS);
   if(!isTerminated)
   { // print message to show user which thread is blocking
      if(logger)
         logger->log(Log_WARNING, logContext,
            "Still waiting for this component to stop: " + component->getName() );

      component->join();

      if(logger)
         logger->log(Log_WARNING, logContext, "Component stopped: " + component->getName() );
   }
}
