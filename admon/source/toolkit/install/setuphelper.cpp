#include <program/Program.h>
#include <toolkit/bashtk.h>
#include <toolkit/webtk.h>
#include "setuphelper.h"

#include <mutex>

/**
 * @return 0 on success (or if file didn't exist), !=0 otherwise
 */
int setuphelper::saveRemoveFile(std::string filename)
{
   int removeRes = remove(filename.c_str() );

   if(removeRes && (errno != ENOENT) )
      return removeRes;

   return 0;
}

int setuphelper::writeConfigFile(std::map<std::string, std::string> *paramsMap)
{
   if (saveRemoveFile(CONFIG_FILE_PATH) != 0)
      return -1;

   std::ofstream confFile(CONFIG_FILE_PATH.c_str(), std::ios::app);
   if (!(confFile.is_open() ) )
      return -1;

   for (std::map<std::string, std::string>::iterator iter = paramsMap->begin(); iter
      != paramsMap->end(); iter++)
   {
      confFile << (*iter).first + "=" + (*iter).second + "\n";
   }

   StringList mgmtdHosts;
   readRolesFile(ROLETYPE_Mgmtd, &mgmtdHosts);
   if (mgmtdHosts.empty())
   {
      confFile << "sysMgmtdHost=\n";
   }
   else
   {
      confFile << "sysMgmtdHost=" + mgmtdHosts.front() + "\n";
   }

   confFile.close();

   return 0;
}

int setuphelper::readConfigFile(std::string *paramsStr)
{
   *paramsStr = "";
   std::ifstream confFile(CONFIG_FILE_PATH.c_str() );

   if (!(confFile.is_open()))
      return -1;

   while (!confFile.eof())
   {
      std::string line;
      getline(confFile, line);
      *paramsStr += line + "&";
   }

   confFile.close();
   return 0;
}

/*
 * This method is unused in the moment but in the future with the fix it is helpful to update the
 * configuration with out a new installation of all components.
 */
int setuphelper::changeConfig(std::map<std::string, std::string> *paramsMap,
   StringList *failedNodes)
{

   saveRemoveFile(FAILED_NODES_PATH);
   if (saveRemoveFile(CONFIG_FILE_PATH) != 0)
      return -1;

   std::ofstream confFile(CONFIG_FILE_PATH.c_str(), std::ios::app);
   if (!(confFile.is_open()))
      return -1;

   for (std::map<std::string, std::string>::iterator iter = paramsMap->begin(); iter
      != paramsMap->end(); iter++)
   {
      confFile << (*iter).first + "=" + (*iter).second + "\n";
   }

   StringList mgmtdHosts;
   readRolesFile(ROLETYPE_Mgmtd, &mgmtdHosts);
   confFile << "sysMgmtdHost=" + mgmtdHosts.front() + "\n";

   confFile.close();

   return 0;
}

int setuphelper::readConfigFile(std::map<std::string, std::string> *paramsMap)
{
   std::ifstream confFile(CONFIG_FILE_PATH.c_str() );
   if (!(confFile.is_open()))
      return -1;

   while (!confFile.eof())
   {
      std::string line;
      getline(confFile, line);
      StringList lineSplit;
      StringTk::explode(line, '=', &lineSplit);

      if (lineSplit.size() > 1)
         (*paramsMap)[lineSplit.front()] = lineSplit.back();
      else
      if (lineSplit.size() == 1)
         (*paramsMap)[lineSplit.front()] = "";
   }

   confFile.close();
   return 0;
}

int setuphelper::writeRolesFile(RoleType roleType, StringList *nodeNames)
{
   std::string filename;

   switch (roleType)
   {
      case ROLETYPE_Meta:
         filename = META_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Storage:
         filename = STORAGE_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Client:
         filename = CLIENT_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Mgmtd:
         filename = MGMTD_ROLE_FILE_PATH;
         break;
   }

   if (saveRemoveFile(filename) != 0)
      return -1;

   std::ofstream rolesFile(filename.c_str(), std::ios::app);
   if (!(rolesFile.is_open()))
      return -1;

   for (StringListIter iter = nodeNames->begin(); iter != nodeNames->end(); iter++)
   {
      rolesFile << (*iter) + "\n";
   }

   rolesFile.close();

   // rewrite the config if the mgmtd has changed
   if (roleType == ROLETYPE_Mgmtd)
   {
      std::string paramsStr;
      if(readConfigFile(&paramsStr) )
         return -1;

      StringMap paramsMap;
      webtk::paramsToMap(paramsStr, &paramsMap);

      if(writeConfigFile(&paramsMap) )
         return -1;
   }


   return 0;
}

int setuphelper::readRolesFile(RoleType roleType, StringList *outNodeNames)
{
   std::string filename;

   switch (roleType)
   {
      case ROLETYPE_Meta:
         filename = META_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Storage:
         filename = STORAGE_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Client:
         filename = CLIENT_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Mgmtd:
         filename = MGMTD_ROLE_FILE_PATH;
         break;
   }

   std::ifstream rolesFile(filename.c_str());
   if (!(rolesFile.is_open()))
   {
      return -1;
   }

   while (!rolesFile.eof())
   {
      std::string line;
      getline(rolesFile, line);
      StringList splitLine;
      StringTk::explode(line, ',', &splitLine);

      if (!splitLine.empty())
      {
         std::string node = StringTk::trim(splitLine.front());
         if (!node.empty())
         {
            outNodeNames->push_back(node);
         }
      }
   }

   rolesFile.close();
   return 0;
}

int setuphelper::readRolesFile(RoleType roleType, StringVector *outNodeNames)
{
   StringList tmpList;
   int ret = readRolesFile(roleType, &tmpList);

   for (StringListIter iter = tmpList.begin(); iter != tmpList.end(); iter++)
   {
      outNodeNames->push_back(*iter);
   }

   return ret;
}

int setuphelper::writeIBFile(std::map<std::string, std::string> *paramsMap)
{
   if (saveRemoveFile(IB_CONFIG_FILE_PATH) != 0)
      return -1;

   std::ofstream ibConfFile(IB_CONFIG_FILE_PATH.c_str(), std::ios::app);
   if (!(ibConfFile.is_open()))
      return -1;

   StringList hostList;

   for (std::map<std::string, std::string>::iterator iter = paramsMap->begin();
      iter != paramsMap->end();
      iter++)
   {
      std::string line = (*iter).first;
      StringList tmpList;
      StringTk::explode(line, '_', &tmpList);
      std::string host = tmpList.front();
      hostList.push_back(host);
   }

   hostList.sort();
   hostList.unique();

   for (StringListIter iter = hostList.begin(); iter != hostList.end(); iter++)
   {
      if (StringTk::strToBool((*paramsMap)[std::string((*iter) + "_useIB")]))
         ibConfFile << (*iter) << ",OFED_KERNEL_INCLUDE_PATH=" << (*paramsMap)[std::string((*iter)
            + "_kernelibincpath")] << "\n";
   }

   ibConfFile.close();
   return 0;
}

int setuphelper::readIBFile(StringList *hosts, StringMap *outMap)
{
   std::ifstream ibConfFile(IB_CONFIG_FILE_PATH.c_str() );

   if (!(ibConfFile.is_open()))
      return -1;

   while (!ibConfFile.eof())
   {
      std::string line;
      getline(ibConfFile, line);
      StringList splitLine;
      StringTk::explode(line, ',', &splitLine);
      if (!splitLine.empty())
      {
         std::string node = StringTk::trim(splitLine.front());
         hosts->push_back(node);
         StringList splitOptions;
         StringTk::explode(splitLine.back(), ' ', &splitOptions);

         StringList splitParam;
         StringTk::explode(splitOptions.front(), '=', &splitParam);
         splitOptions.pop_front();

         if (splitParam.size() > 1)
         {
            (*outMap)[std::string((node) + "_kernelibincpath")] = splitParam.back();
         }
         else
         {
            (*outMap)[std::string((node) + "_kernelibincpath")] = "";
         }
      }
   }

   ibConfFile.close();
   return 0;
}

int setuphelper::getInstallNodeInfo(InstallNodeInfoList *metaInfo,
   InstallNodeInfoList *storageInfo, InstallNodeInfoList *clientInfo)
{
   Logger* log = Logger::getLogger();

   int sum = 0;
   int ret = 0;

   ret = getInstallNodeInfo(ROLETYPE_Meta, metaInfo);
   if(ret < 0)
      log->log(Log_ERR, __func__, "Failed to get installation information of the metadata nodes.");
   else
      sum += ret;

   ret = getInstallNodeInfo(ROLETYPE_Storage, storageInfo);
   if(ret < 0)
      log->log(Log_ERR, __func__, "Failed to get installation information of the metadata nodes.");
   else
      sum += ret;

   ret = getInstallNodeInfo(ROLETYPE_Client, clientInfo);
   if(ret < 0)
      log->log(Log_ERR, __func__, "Failed to get installation information of the metadata nodes.");
   else
      sum += ret;

   return sum;
}

int setuphelper::getInstallNodeInfo(InstallNodeInfoList *metaInfo,
   InstallNodeInfoList *storageInfo, InstallNodeInfoList *clientInfo,
   InstallNodeInfoList *mgmtdInfo)
{
   Logger* log = Logger::getLogger();

   int sum = 0;
   sum = getInstallNodeInfo(metaInfo, storageInfo, clientInfo);

   int ret = getInstallNodeInfo(ROLETYPE_Mgmtd, mgmtdInfo);
   if(ret < 0)
      log->log(Log_ERR, __func__, "Failed to get installation information of the management node.");
   else
      sum += ret;

   return ret;
}

int setuphelper::getInstallNodeInfo(RoleType roleType, InstallNodeInfoList *info)
{
   std::string filename;

   switch (roleType)
   {
      case ROLETYPE_Meta:
         filename = META_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Storage:
         filename = STORAGE_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Client:
         filename = CLIENT_ROLE_FILE_PATH;
         break;
      case ROLETYPE_Mgmtd:
         filename = MGMTD_ROLE_FILE_PATH;
         break;
   }

   std::ifstream rolesFile(filename.c_str());
   if (!(rolesFile.is_open()))
   {
      return -1;
   }

   while (!rolesFile.eof())
   {
      std::string line;
      getline(rolesFile, line);

      if (!StringTk::trim(line).empty())
      {
         StringList splitLine;
         StringTk::explode(line, ',', &splitLine);
         InstallNodeInfo nodeInfo;
         StringListIter iter = splitLine.begin();

         if (iter == splitLine.end())
            return -1;

         nodeInfo.name = StringTk::trim(*iter);

         if (++iter == splitLine.end())
            return -1;

         std::string arch = *iter;

         if (arch.compare("i586") == 0)
            nodeInfo.arch = ARCH_32;
         else
         if (arch.compare("x86_64") == 0)
            nodeInfo.arch = ARCH_64;
         else
         if (arch.compare("ppc") == 0)
            nodeInfo.arch = ARCH_ppc32;
         else
         if (arch.compare("ppc64") == 0)
            nodeInfo.arch = ARCH_ppc64;

         if (++iter == splitLine.end())
            return -1;

         nodeInfo.distributionName = *iter;
         if (++iter == splitLine.end())
            return -1;

         nodeInfo.distributionTag = *iter;
         info->push_back(nodeInfo);
      }
   }

   rolesFile.close();

   return 0;
}

int setuphelper::createRepos(StringList *failedNodes)
{
   ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

   saveRemoveFile(FAILED_NODES_PATH);

   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock(mutex);

      job = jobRunner->addJob(BASH + " " + SCRIPT_REPO_PARALLEL + " " + FAILED_NODES_PATH, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);
   }

   if (job->returnCode != 0)
   {
      jobRunner->delJob(job->id);
      return -1;
   }

   jobRunner->delJob(job->id);

   if(checkFailedNodesFile(failedNodes) )
      return -1;

   return 0;
}

int setuphelper::createNewSetupLogfile(std::string type)
{
   std::string mgmtdString;
   std::string metaString;
   std::string storageString;
   std::string clientsString;
   std::string field;
   std::string line;

   std::ifstream finStorage(STORAGE_ROLE_FILE_PATH.c_str() );
   while (std::getline(finStorage, line))
   {
      std::istringstream linestream(line);
      std::getline(linestream, field, ',');
      storageString = storageString + field + ", ";
   }
   finStorage.close();

   std::ifstream finClient(CLIENT_ROLE_FILE_PATH.c_str() );
   while (std::getline(finClient, line))
   {
      std::istringstream linestream(line);
      std::getline(linestream, field, ',');
      clientsString = clientsString + field + ", ";
   }
   finClient.close();

   std::ifstream finMgmtd(MGMTD_ROLE_FILE_PATH.c_str() );
   while (std::getline(finMgmtd, line))
   {
      std::istringstream linestream(line);
      std::getline(linestream, field, ',');
      mgmtdString = mgmtdString + field + ", ";
   }
   finMgmtd.close();

   std::ifstream finMeta(META_ROLE_FILE_PATH.c_str() );
   while (std::getline(finMeta, line))
   {
      std::istringstream linestream(line);
      std::getline(linestream, field, ',');
      metaString = metaString + field + ", ";
   }
   finMeta.close();

   std::string destPath(SETUP_LOG_PATH);
   destPath.append(".old");
   int retValue = rename(SETUP_LOG_PATH.c_str(), destPath.c_str() );
   if(retValue)
      Logger::getLogger()->log(Log_ERR, __func__, "Failed to move file " + SETUP_LOG_PATH
         + " to " + destPath);

   std::ofstream fout(SETUP_LOG_PATH.c_str() );
   fout << "--------------------------------------------" << std::endl;

   if (type == "install")
   {
      fout << "starting installation..."  << std::endl;
   } else
   {
      fout << "starting uninstallation..."  << std::endl;
   }

   fout << "--------------------------------------------" << std::endl;
   fout << "Managment server: " << mgmtdString << std::endl;
   fout << "Metadata server: " << metaString << std::endl;
   fout << "Storage server: " << storageString << std::endl;
   fout << "Clients: " << clientsString << std::endl;
   fout << "--------------------------------------------" << std::endl;
   fout << "--------------------------------------------" << std::endl;
   fout << std::endl;
   fout.close();

   return retValue;
}

int setuphelper::installSinglePackage(std::string package, std::string host)
{
   ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock(mutex);

      job = jobRunner->addJob(BASH + " " + SCRIPT_INSTALL + " " + package + " " + host, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);
   }

   if(job->returnCode != 0)
   {
      jobRunner->delJob(job->id);
      return -1;
   }

   jobRunner->delJob(job->id);
   return 0;
}

int setuphelper::installPackage(std::string package, StringList *failedNodes)
{
   ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

   saveRemoveFile(FAILED_NODES_PATH);

   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock(mutex);

      job = jobRunner->addJob(BASH + " " + SCRIPT_INSTALL_PARALLEL + " " + package + " " +
         FAILED_NODES_PATH, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);
   }

   if (job->returnCode != 0)
   {
      jobRunner->delJob(job->id);
      return -1;
   }

   jobRunner->delJob(job->id);

   if(checkFailedNodesFile(failedNodes) )
      return -1;

   return 0;
}

int setuphelper::uninstallSinglePackage(std::string package, std::string host)
{
   ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock(mutex);

      job = jobRunner->addJob(BASH + " " + SCRIPT_UNINSTALL + " " + package + " " + host, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);
   }

   if (job->returnCode != 0)
   {
      jobRunner->delJob(job->id);
      return -1;
   }

   jobRunner->delJob(job->id);
   return 0;
}

int setuphelper::uninstallPackage(std::string package, StringList *failedNodes)
{
   ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

   saveRemoveFile(FAILED_NODES_PATH);

   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock(mutex);

      job = jobRunner->addJob(BASH + " " + SCRIPT_UNINSTALL_PARALLEL + " " + package + " " +
         FAILED_NODES_PATH, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);
   }

   if (job->returnCode != 0)
   {
      jobRunner->delJob(job->id);
      return -1;
   }

   jobRunner->delJob(job->id);

   if(checkFailedNodesFile(failedNodes) )
      return -1;

   return 0;
}

int setuphelper::checkDistriArch()
{
   ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock(mutex);

      job = jobRunner->addJob(BASH + " " + SCRIPT_SYSTEM_INFO_PARALLEL + " ", &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);
   }

   if (job->returnCode != 0)
   {
      jobRunner->delJob(job->id);
      return -1;
   }

   jobRunner->delJob(job->id);
   return 0;
}

int setuphelper::checkSSH(StringList *hosts, StringList *failedNodes)
{
   ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

   std::string hostList;

   for (StringListIter iter = hosts->begin(); iter != hosts->end(); iter++)
   {
      hostList = *iter + "," + hostList;
   }

   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock (mutex);

      job = jobRunner->addJob(BASH + " " + SCRIPT_CHECK_SSH_PARALLEL + " " + hostList, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);
   }

   if (job->returnCode != 0)
   {
      jobRunner->delJob(job->id);
      return -1;
   }

   std::ifstream failedNodesFile(FAILED_SSH_CONNECTIONS.c_str() );
   if (!(failedNodesFile.is_open()))
   {
      return -2;
   }

   while (!failedNodesFile.eof())
   {
      std::string line;
      getline(failedNodesFile, line);
      if (!StringTk::trim(line).empty())
      {
         failedNodes->push_back(line);
      }
   }

   failedNodesFile.close();
   if (!failedNodes->empty())
   {
      return -1;
   }

   saveRemoveFile(FAILED_SSH_CONNECTIONS);

   jobRunner->delJob(job->id);

   return 0;
}

int setuphelper::updateAdmonConfig()
{
   ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock(mutex);

      std::string cfgFile = Program::getApp()->getConfig()->getCfgFile();

      job = jobRunner->addJob(BASH + " " + SCRIPT_WRITE_CONFIG + " localhost " + cfgFile + " " +
         SETUP_LOG_PATH, &mutex);

      while(!job->finished)
         job->jobFinishedCond.wait(&mutex);
   }

   Program::getApp()->getConfig()->resetMgmtdDaemon();

   if (job->returnCode != 0)
   {
      jobRunner->delJob(job->id);
      return -1;
   }

   jobRunner->delJob(job->id);
   return 0;
}

/**
 * @return number of failed node or -1 on error
 */
int setuphelper::checkFailedNodesFile(StringList *failedNodes)
{
   // no failed nodes written to failedNodes file ==> no failed nodes
   if(!StorageTk::pathExists(FAILED_NODES_PATH) )
      return 0;

   std::ifstream failedNodesFile(FAILED_NODES_PATH.c_str() );
   if (!(failedNodesFile.is_open() ) )
   {
      return -1;
   }

   while (!failedNodesFile.eof() )
   {
      std::string line;
      getline(failedNodesFile, line);

      if (!StringTk::trim(line).empty())
      {
         failedNodes->push_back(line);
      }
   }

   saveRemoveFile(FAILED_NODES_PATH);

   return failedNodes->size();
}
