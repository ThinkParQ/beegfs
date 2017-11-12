#ifndef SETUPHELPER_H
#define SETUPHELPER_H

/*
 *  SetupFunctions is a collection of static functions to perform all kinds of
 *  things related to admon-based graphical BeeGFS setup. Many functions here
 *  call external scripts of the beegfs_setup package
 */

#include <common/toolkit/StringTk.h>
#include <common/nodes/Node.h>
#include <components/ExternalJobRunner.h>
#include <common/Common.h>



enum RoleType
{
   ROLETYPE_Meta = 1,
   ROLETYPE_Storage = 2,
   ROLETYPE_Client = 3,
   ROLETYPE_Mgmtd = 4
};

enum Arch
{
   ARCH_32 = 1, ARCH_64 = 2, ARCH_ppc32 = 3, ARCH_ppc64 = 4
};


struct InstallNodeInfo
{
      std::string name;
      Arch arch;
      std::string distributionName;
      std::string distributionTag;
};


typedef std::list<InstallNodeInfo> InstallNodeInfoList;
typedef InstallNodeInfoList::iterator InstallNodeInfoListIter;


namespace setuphelper
{
   int writeConfigFile(std::map<std::string, std::string> *paramsMap);
   int writeIBFile(std::map<std::string, std::string> *paramsMap);
   int readIBFile(StringList *hosts, StringMap *outMap);
   int readConfigFile(std::string *paramsStr);
   int readConfigFile(std::map<std::string, std::string> *paramsMap);
   int changeConfig(std::map<std::string, std::string> *paramsMap, StringList *failedNodes);
   int writeRolesFile(RoleType roleType, StringList *nodeNames);
   int readRolesFile(RoleType roleType, StringList *nodeNames);
   int readRolesFile(RoleType roleType, StringVector *nodeNames);
   int updateAdmonConfig();

   int detectIBPaths(std::string host, StringList *incPaths, StringList *libPaths,
      StringList *kernelIncPaths);
   int detectIBPaths(std::string host, std::string *incPath, std::string *libPath,
      std::string *kernelIncPath);

   int getInstallNodeInfo(InstallNodeInfoList *metaInfo, InstallNodeInfoList *storageInfo,
      InstallNodeInfoList *clientInfo);
   int getInstallNodeInfo(InstallNodeInfoList *metaInfo, InstallNodeInfoList *storageInfo,
      InstallNodeInfoList *clientInfo, InstallNodeInfoList *mgmtdInfo);
   int getInstallNodeInfo(RoleType roleType, InstallNodeInfoList *info);

   int createRepos(StringList *failedNodes);
   int createNewSetupLogfile(std::string type);

   int installPackage(std::string package, StringList *failedNodes);
   int installSinglePackage(std::string package, std::string host);
   int uninstallPackage(std::string package, StringList *failedNodes);
   int uninstallSinglePackage(std::string package, std::string host);

   int checkDistriArch();
   int checkSSH(StringList *hosts, StringList *failedNodes);
   int checkFailedNodesFile(StringList *failedNodes);

   int saveRemoveFile(std::string filename);
};

#endif /* SETUPHELPER_H */
