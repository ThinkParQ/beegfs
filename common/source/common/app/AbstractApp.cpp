#include <common/toolkit/StorageTk.h>
#include <common/toolkit/NodesTk.h>
#include "AbstractApp.h"

bool AbstractApp::didRunTimeInit = false;

/**
 * Note: Will do nothing if pidFile string is empty.
 *
 * @pidFile will not be accepted if path is not absolute (=> throws exception).
 * @return LockFD for locked pid file or invalid if pidFile string was empty.
 */
LockFD AbstractApp::createAndLockPIDFile(const std::string& pidFile)
{
   if (pidFile.empty())
      return {};

   // PID file defined

   Path pidPath(pidFile);
   if(!pidPath.absolute() ) /* (check to avoid problems after chdir) */
      throw InvalidConfigException("Path to PID file must be absolute: " + pidFile);

   auto pidFileLockFD = StorageTk::createAndLockPIDFile(pidFile, true);
   if (!pidFileLockFD.valid())
      throw InvalidConfigException("Unable to create/lock PID file: " + pidFile);

   return pidFileLockFD;
}

/**
 * Updates a locked pid file, which is typically required after calling daemon().
 *
 * Note: Will do nothing if pidfileFD is invalid.
 */
void AbstractApp::updateLockedPIDFile(LockFD& pidFileFD)
{
   if (!pidFileFD.valid())
      return;

   if (auto err = pidFileFD.updateWithPID())
      throw InvalidConfigException("Unable to update PID file: " + err.message());
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
   if (!timeTest)
      return false;

   bool conditionTimeTest = Condition::testClockID();
   return conditionTimeTest;
}

bool AbstractApp::basicInitializations()
{
   bool condAttrRes = Condition::initStaticCondAttr();
   return condAttrRes;
}

bool AbstractApp::basicDestructions()
{
   bool condAttrRes = Condition::destroyStaticCondAttr();
   return condAttrRes;
}

void AbstractApp::logUsableNICs(LogContext* log, NicAddressList& nicList)
{
   // list usable network interfaces
   std::string nicListStr;
   std::string extendedNicListStr;

   for(NicAddressListIter nicIter = nicList.begin(); nicIter != nicList.end(); nicIter++)
   {
      std::string nicTypeStr;

      if (nicIter->nicType == NICADDRTYPE_RDMA)
         nicTypeStr = "RDMA";
      else if (nicIter->nicType == NICADDRTYPE_SDP)
         nicTypeStr = "SDP";
      else if (nicIter->nicType == NICADDRTYPE_STANDARD)
         nicTypeStr = "TCP";
      else
         nicTypeStr = "Unknown";

      nicListStr += std::string(nicIter->name) + "(" + nicTypeStr + ")" + " ";

      extendedNicListStr += "\n+ ";
      extendedNicListStr += NetworkInterfaceCard::nicAddrToString(&(*nicIter) ) + " ";
   }

   nicListStr = std::string("Usable NICs: ") + nicListStr;
   extendedNicListStr = std::string("Extended list of usable NICs: ") + extendedNicListStr;

   if (log)
   {
      log->log(Log_WARNING, nicListStr);
      log->log(Log_DEBUG, extendedNicListStr);
   }
   else
   {
      LOG(GENERAL, WARNING, nicListStr);
      LOG(GENERAL, DEBUG, extendedNicListStr);
   }
}

void AbstractApp::updateLocalNicListAndRoutes(LogContext* log, NicAddressList& localNicList,
   std::vector<AbstractNodeStore*>& nodeStores)
{
   bool needRoutes = getCommonConfig()->getConnRestrictOutboundInterfaces();

   if (needRoutes)
      updateRoutingTable();

   std::unique_lock<Mutex> lock(localNicListMutex);
   this->localNicList = localNicList;
   lock.unlock();

   logUsableNICs(log, localNicList);
   NicListCapabilities localNicCaps;
   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);

   for (auto& l : nodeStores)
   {
      NodesTk::applyLocalNicListToList(localNicList, localNicCaps,
         l->referenceAllNodes());
   }
}

bool AbstractApp::initNoDefaultRouteList(NetVector* outNets)
{
   std::string connNoDefaultRoute = getCommonConfig()->getConnNoDefaultRoute();
   StringVector cidrs;
   StringTk::explodeEx(connNoDefaultRoute, ',', true, &cidrs);
   for (auto& c : cidrs)
   {
      IPv4Network net;
      if (!IPv4Network::parse(c, net))
         return false;
      outNets->push_back(net);
   }
   return true;
}

/**
 * Logs a message that explains the terms of the EULA regarding the use of Enterprise Features
 * and other use cases that require a License & Support Agreement.
 * @param enabledFeatures string containing the Enterprise Features that are enabled in the
 * specific BeeGFS service. Will be included in the output if not empty.
 */
void AbstractApp::logEULAMsg(std::string enabledFeatures)
{
   if (!enabledFeatures.empty())
      enabledFeatures.resize(72, ' ');
   LOG(GENERAL, WARNING, "--------------------------------------------------------------------------------");
   LOG(GENERAL, WARNING, "| BeeGFS End User License Agreement                                            |");
   LOG(GENERAL, WARNING, "|                                                                              |");
   LOG(GENERAL, WARNING, "| By downloading and/or installing BeeGFS, you have agreed to the EULA of      |");
   LOG(GENERAL, WARNING, "| BeeGFS: https://www.beegfs.io/docs/BeeGFS_EULA.txt                           |");
   LOG(GENERAL, WARNING, "|                                                                              |");
   LOG(GENERAL, WARNING, "| Please note that the following use cases of BeeGFS are only allowed if you   |");
   LOG(GENERAL, WARNING, "| have a valid License & Support Agreement with the licensor of BeeGFS         |");
   LOG(GENERAL, WARNING, "| \"ThinkParQ GmbH\":                                                            |");
   LOG(GENERAL, WARNING, "|                                                                              |");
   LOG(GENERAL, WARNING, "|  - The use of any Enterprise Features of BeeGFS for longer than the trial    |");
   if (enabledFeatures.empty())
      LOG(GENERAL, WARNING, "|    period of 60 (sixty) days (§3.4 EULA).                                    |");
   else {
      LOG(GENERAL, WARNING, "|    period of 60 (sixty) days (§3.4 EULA). The following Enterprise Features  |");
      LOG(GENERAL, WARNING, "|    are currently in use:                                                     |");
      LOG(GENERAL, WARNING, "|      " + enabledFeatures + "|");
   }
   LOG(GENERAL, WARNING, "|  - The use of BeeGFS as part of a commercial turn-key solution (§3.3 EULA)   |");
   LOG(GENERAL, WARNING, "|  - Any commercial services for the product BeeGFS by a third party           |");
   LOG(GENERAL, WARNING, "|    (§3.3 EULA)                                                               |");
   LOG(GENERAL, WARNING, "|                                                                              |");
   LOG(GENERAL, WARNING, "| Contact: sales@thinkparq.com                                                 |");
   LOG(GENERAL, WARNING, "| Thank you for supporting BeeGFS development!                                 |");
   LOG(GENERAL, WARNING, "--------------------------------------------------------------------------------");
}
