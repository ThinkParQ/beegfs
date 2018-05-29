#include <common/toolkit/StorageTk.h>
#include "AbstractApp.h"

bool AbstractApp::didRunTimeInit = false;

/**
 * Note: Will do nothing if pidFile string is empty.
 *
 * @pidFile will not be accepted if path is not absolute (=> throws exception).
 * @return file descriptor for locked pid file or -1 iff pidFile string was empty.
 */
int AbstractApp::createAndLockPIDFile(std::string pidFile)
{
   if(pidFile.empty() )
      return -1;

   // PID file defined

   Path pidPath(pidFile);
   if(!pidPath.absolute() ) /* (check to avoid problems after chdir) */
      throw InvalidConfigException("Path to PID file must be absolute: " + pidFile);

   int pidFileLockFD = StorageTk::createAndLockPIDFile(pidFile, true);
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
void AbstractApp::updateLockedPIDFile(int pidFileFD)
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
void AbstractApp::unlockAndDeletePIDFile(int pidFileFD, const std::string& pidFile)
{
   if(pidFileFD == -1)
      return;

   StorageTk::unlockAndDeletePIDFile(pidFileFD, pidFile);
}


/**
 * @param component the thread that we're waiting for via join(); may be NULL (in which case this
 * method returns immediately)
 */
void AbstractApp::waitForComponentTermination(PThread* component)
{
   const char* logContext = "App (wait for component termination)";
   Logger* logger = Logger::getLogger();

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

/**
 * Handle errors from new operator.
 *
 * This method is intended by the C++ standard to try to free up some memory after allocation
 * failed, but our options here are very limited, because we don't know which locks are being held
 * when this is called and how much memory the caller actually requested.
 *
 * @return this function should only return if it freed up memory (otherwise it might be called
 * infinitely); as we don't free up any memory here, we exit by throwing a std::bad_alloc
 * exeception.
 * @throw std::bad_alloc always.
 */
void AbstractApp::handleOutOfMemFromNew()
{
   int errCode = errno;

   std::cerr << "Failed to allocate memory via \"new\" operator. "
      "Will try to log a backtrace and then throw a bad_alloc exception..." << std::endl;
   std::cerr << "Last errno value: " << errno << std::endl;

   LogContext log(__func__);
   log.logErr("Failed to allocate memory via \"new\" operator. "
      "Will try to log a backtrace and then throw a bad_alloc exception...");

   log.logErr("Last errno value: " + System::getErrString(errCode) );

   log.logBacktrace();

   throw std::bad_alloc();
}

bool AbstractApp::performBasicInitialRunTimeChecks()
{
   bool timeTest = Time::testClock();
   if (timeTest == false)
      return false;

   bool conditionTimeTest = Condition::testClockID();
   if (conditionTimeTest == false)
      return false;

   return true;
}

bool AbstractApp::basicInitializations()
{
   bool condAttrRes = Condition::initStaticCondAttr();
   if (condAttrRes == false)
      return false;

   return true;
}

bool AbstractApp::basicDestructions()
{
   bool condAttrRes = Condition::destroyStaticCondAttr();
   if (condAttrRes == false)
      return false;

   return true;
}

/**
 * Returns highest feature bit number found in array.
 *
 * @param numArrayElems must be larger than zero.
 */
unsigned AbstractApp::featuresGetHighestNum(const unsigned* featuresArray, unsigned numArrayElems)
{
   unsigned maxBit = 0;

   for(unsigned i=0; i < numArrayElems; i++)
   {
      if(featuresArray[i] > maxBit)
         maxBit = featuresArray[i];
   }

   return maxBit;
}

/**
 * Walk over featuresArray and add found feature bits to outBitStore.
 *
 * @param outBitStore will be resized and cleared before bits are set.
 */
void AbstractApp::featuresToBitStore(const unsigned* featuresArray, unsigned numArrayElems,
   BitStore* outBitStore)
{
   unsigned maxFeatureBit = featuresGetHighestNum(featuresArray, numArrayElems);

   outBitStore->setSize(maxFeatureBit);
   outBitStore->clearBits();

   for(unsigned i=0; i < numArrayElems; i++)
      outBitStore->setBit(featuresArray[i], true);
}
