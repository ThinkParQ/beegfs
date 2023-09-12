#include "App.h"

#include <app/SignalHandler.h>
#include <common/components/ComponentInitException.h>
#include <common/components/worker/DummyWork.h>
#include <misc/Cassandra.h>
#include <misc/InfluxDB.h>


App::App(int argc, char** argv) :
   argc(argc), argv(argv)
{}

void App::run()
{
   try
   {
      cfg = boost::make_unique<Config>(argc,argv);
      runNormal();
      appResult = AppCode::NO_ERROR;
   }
   catch (const InvalidConfigException& e)
   {
      std::ostringstream err;
      err << "Config error: " << e.what() << std::endl
            << "[BeeGFS Mon Version: " << BEEGFS_VERSION << std::endl
            << "Refer to the default config file (/etc/beegfs/beegfs-mon.conf)" << std::endl
            << "or visit http://www.beegfs.com to find out about configuration options.]";
      printOrLogError(err.str());
      appResult = AppCode::INVALID_CONFIG;
   }
   catch (const ComponentInitException& e)
   {
      printOrLogError("Component initialization error: " + std::string(e.what()));
      appResult = AppCode::INITIALIZATION_ERROR;
   }
   catch (const std::runtime_error& e)
   {
      printOrLogError("Runtime error: " + std::string(e.what()));
      appResult = AppCode::RUNTIME_ERROR;
   }
   catch (const std::exception& e)
   {
      printOrLogError("Generic error: " + std::string(e.what()));
      appResult = AppCode::RUNTIME_ERROR;
   }
}

void App::printOrLogError(const std::string& text) const
{
   if (Logger::isInitialized())
      LOG(GENERAL, ERR, text);
   else
      std::cerr << std::endl << text << std::endl << std::endl;
}

void App::runNormal()
{
   Logger::createLogger(cfg->getLogLevel(), cfg->getLogType(), cfg->getLogNoDate(),
         cfg->getLogStdFile(), cfg->getLogNumLines(), cfg->getLogNumRotatedFiles());

   pidFileLockFD = createAndLockPIDFile(cfg->getPIDFile());
   initDataObjects();
   SignalHandler::registerSignalHandler(this);
   initLocalNodeInfo();
   initWorkers();
   initComponents();

   RDMASocket::rdmaForkInitOnce();


   if (cfg->getRunDaemonized())
      daemonize();

   logInfos();

   // make sure components don't receive SIGINT/SIGTERM (blocked signals are inherited)
   PThread::blockInterruptSignals();
   startWorkers();
   startComponents();
   PThread::unblockInterruptSignals();

   joinComponents();
   joinWorkers();
}

void App::initLocalNodeInfo()
{
   bool useRDMA = cfg->getConnUseRDMA();
   unsigned portUDP = cfg->getConnMonPortUDP();

   StringList allowedInterfaces;
   std::string interfacesFilename = cfg->getConnInterfacesFile();
   if (interfacesFilename.length() )
      cfg->loadStringListFile(interfacesFilename.c_str(), allowedInterfaces);

   NetworkInterfaceCard::findAll(&allowedInterfaces, useRDMA, &localNicList);

   if (localNicList.empty() )
      throw InvalidConfigException("Couldn't find any usable NIC");

   localNicList.sort(NetworkInterfaceCard::NicAddrComp{&allowedInterfaces});
   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);

   noDefaultRouteNets = std::make_shared<NetVector>();
   if(!initNoDefaultRouteList(noDefaultRouteNets.get()))
      throw InvalidConfigException("Failed to parse connNoDefaultRoute");

   initRoutingTable();
   updateRoutingTable();

   std::string nodeID = System::getHostname();

   // TODO add a Mon nodetype at some point
   localNode = std::make_shared<LocalNode>(NODETYPE_Client, nodeID, NumNodeID(1), portUDP, 0, localNicList);
}

void App::initDataObjects()
{
   netFilter = boost::make_unique<NetFilter>(cfg->getConnNetFilterFile());
   tcpOnlyFilter = boost::make_unique<NetFilter>(cfg->getConnTcpOnlyFilterFile());
   netMessageFactory = boost::make_unique<NetMessageFactory>();
   workQueue = boost::make_unique<MultiWorkQueue>();

   targetMapper = boost::make_unique<TargetMapper>();

   metaNodes = boost::make_unique<NodeStoreMetaEx>();
   storageNodes = boost::make_unique<NodeStoreStorageEx>();
   mgmtNodes = boost::make_unique<NodeStoreMgmtEx>();

   metaBuddyGroupMapper = boost::make_unique<MirrorBuddyGroupMapper>();
   storageBuddyGroupMapper = boost::make_unique<MirrorBuddyGroupMapper>();


   if (cfg->getDbType() == Config::DbTypes::CASSANDRA)
   {
      Cassandra::Config cassandraConfig;
      cassandraConfig.host = cfg->getDbHostName();
      cassandraConfig.port = cfg->getDbHostPort();
      cassandraConfig.database = cfg->getDbDatabase();
      cassandraConfig.maxInsertsPerBatch = cfg->getCassandraMaxInsertsPerBatch();
      cassandraConfig.TTLSecs = cfg->getCassandraTTLSecs();

      tsdb = boost::make_unique<Cassandra>(std::move(cassandraConfig));
   }
   else // Config::DbTypes::INFLUXDB OR Config::DbTypes::INFLUXDB2
   {
      InfluxDB::Config influxdbConfig;
      influxdbConfig.host = cfg->getDbHostName();
      influxdbConfig.port = cfg->getDbHostPort();
      influxdbConfig.maxPointsPerRequest = cfg->getInfluxdbMaxPointsPerRequest();
      influxdbConfig.httpTimeout = cfg->getHttpTimeout();
      influxdbConfig.curlCheckSSLCertificates = cfg->getCurlCheckSSLCertificates();
      if (cfg->getDbType() == Config::DbTypes::INFLUXDB2)
      {
         influxdbConfig.bucket = cfg->getDbBucket();
         influxdbConfig.organization = cfg->getDbAuthOrg();
         influxdbConfig.token = cfg->getDbAuthToken();
         influxdbConfig.dbVersion = INFLUXDB2;
      }
      else
      {
         influxdbConfig.database = cfg->getDbDatabase();
         influxdbConfig.setRetentionPolicy = cfg->getInfluxDbSetRetentionPolicy();
         influxdbConfig.retentionDuration = cfg->getInfluxDbRetentionDuration();
         influxdbConfig.username = cfg->getDbAuthUsername();
         influxdbConfig.password = cfg->getDbAuthPassword();
         influxdbConfig.dbVersion = INFLUXDB;
      }
      tsdb = boost::make_unique<InfluxDB>(std::move(influxdbConfig));
   }
}

void App::initComponents()
{
   nodeListRequestor = boost::make_unique<NodeListRequestor>(this);
   statsCollector = boost::make_unique<StatsCollector>(this);
   cleanUp = boost::make_unique<CleanUp>(this);
}

void App::startComponents()
{
   LOG(GENERAL, DEBUG, "Starting components...");
   nodeListRequestor->start();
   statsCollector->start();
   cleanUp->start();
   LOG(GENERAL, DEBUG, "Components running.");
}

void App::stopComponents()
{
   if (nodeListRequestor)
      nodeListRequestor->selfTerminate();
   if (statsCollector)
      statsCollector->selfTerminate();
   if (cleanUp)
      cleanUp->selfTerminate();

   stopWorkers();
   selfTerminate();
}

void App::joinComponents()
{
   LOG(GENERAL, DEBUG, "Joining Component threads...");
   nodeListRequestor->join();
   statsCollector->join();
   cleanUp->join();
   LOG(GENERAL, CRITICAL, "All components stopped. Exiting now.");
}

void App::initWorkers()
{
   const unsigned numDirectWorkers = 1;
   const unsigned workersBufSize = 1024*1024;

   unsigned numWorkers = cfg->getTuneNumWorkers();

   for (unsigned i=0; i < numWorkers; i++)
   {
      auto worker = boost::make_unique<Worker>("Worker" + StringTk::intToStr(i+1),
            workQueue.get(), QueueWorkType_INDIRECT);

      worker->setBufLens(workersBufSize, workersBufSize);
      workerList.push_back(std::move(worker));
   }

   for (unsigned i=0; i < numDirectWorkers; i++)
   {
      auto worker = boost::make_unique<Worker>("DirectWorker" + StringTk::intToStr(i+1),
            workQueue.get(), QueueWorkType_DIRECT);

      worker->setBufLens(workersBufSize, workersBufSize);
      workerList.push_back(std::move(worker));
   }
}

void App::startWorkers()
{
   for (auto worker = workerList.begin(); worker != workerList.end(); worker++)
   {
      (*worker)->start();
   }
}

void App::stopWorkers()
{
   // need two loops because we don't know if the worker that handles the work will be the same that
   // received the self-terminate-request
   for (auto worker = workerList.begin(); worker != workerList.end(); worker++)
   {
      (*worker)->selfTerminate();

      // add dummy work to wake up the worker immediately for faster self termination
      PersonalWorkQueue* personalQ = (*worker)->getPersonalWorkQueue();
      workQueue->addPersonalWork(new DummyWork(), personalQ);
   }
}

void App::joinWorkers()
{

   for (auto worker = workerList.begin(); worker != workerList.end(); worker++)
   {
      waitForComponentTermination((*worker).get());
   }
}

void App::logInfos()
{
   LOG(GENERAL, CRITICAL, std::string("Version: ") + BEEGFS_VERSION);
#ifdef BEEGFS_DEBUG
   LOG(GENERAL, DEBUG, "--DEBUG VERSION--");
#endif

   // list usable network interfaces
   NicAddressList nicList = getLocalNicList();
   logUsableNICs(NULL, nicList);

   // print net filters
   if (netFilter->getNumFilterEntries() )
   {
      LOG(GENERAL, WARNING, std::string("Net filters: ")
            + StringTk::uintToStr(netFilter->getNumFilterEntries() ) );
   }

   if (tcpOnlyFilter->getNumFilterEntries() )
   {
      LOG(GENERAL, WARNING, std::string("TCP-only filters: ")
            + StringTk::uintToStr(tcpOnlyFilter->getNumFilterEntries() ) );
   }
}

void App::daemonize()
{
   int nochdir = 1; // 1 to keep working directory
   int noclose = 0; // 1 to keep stdin/-out/-err open

   LOG(GENERAL, CRITICAL, "Detaching process...");

   int detachRes = daemon(nochdir, noclose);
   if (detachRes == -1)
      throw std::runtime_error(std::string("Unable to detach process: ")
            + System::getErrString());

   updateLockedPIDFile(pidFileLockFD); // ignored if pidFileFD is -1
}

void App::handleComponentException(std::exception& e)
{
   LOG(GENERAL, CRITICAL, "This component encountered an unrecoverable error.", sysErr,
         ("Exception", e.what()));

   LOG(GENERAL, WARNING, "Shutting down...");
   stopComponents();
}

void App::handleNetworkInterfaceFailure(const std::string& devname)
{
   // Nothing to do. This App has no internodeSyncer that would rescan the
   // netdevs.
   LOG(GENERAL, ERR, "Network interface failure.",
      ("Device", devname));
}
