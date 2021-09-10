#ifndef MSGHELPERGENERICDEBUG_H_
#define MSGHELPERGENERICDEBUG_H_

#include <common/nodes/NodeStoreServers.h>
#include <common/storage/quota/ExceededQuotaStore.h>
#include <common/Common.h>


#define GENDBGMSG_OP_VARLOGMESSAGES       "varlogmessages"
#define GENDBGMSG_OP_VARLOGKERNLOG        "varlogkernlog"
#define GENDBGMSG_OP_FHGFSLOG             "beegfslog"
#define GENDBGMSG_OP_LOADAVG              "loadavg"
#define GENDBGMSG_OP_DROPCACHES           "dropcaches"
#define GENDBGMSG_OP_GETCFG               "getcfg"
#define GENDBGMSG_OP_GETLOGLEVEL          "getloglevel"
#define GENDBGMSG_OP_SETLOGLEVEL          "setloglevel"
#define GENDBGMSG_OP_NETOUT               "net"
#define GENDBGMSG_OP_QUOTAEXCEEDED        "quotaexceeded"
#define GENDBGMSG_OP_USEDQUOTA            "usedquota"
#define GENDBGMSG_OP_LISTMETASTATES       "listmetastates"
#define GENDBGMSG_OP_LISTSTORAGESTATES    "liststoragestates"
#define GENDBGMSG_OP_LISTSTORAGEPOOLS     "liststoragepools"
#define GENDBGMSG_OP_SETREJECTIONRATE     "setrejectionrate"


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
      static std::string processOpNetOut(std::istringstream& commandStream,
         const NodeStoreServers* mgmtNodes, const NodeStoreServers* metaNodes,
         const NodeStoreServers* storageNodes);
      static std::string processOpQuotaExceeded(std::istringstream& commandStream,
         const ExceededQuotaStore* store);
      static std::string processOpListTargetStates(std::istringstream& commandStream,
         const TargetStateStore* targetStateStore);
      static std::string processOpListStoragePools(std::istringstream& commandStream,
         const StoragePoolStore* storagePoolStore);

      static std::string loadTextFile(std::string path);
      static std::string writeTextFile(std::string path, std::string writeStr);


   private:
      MsgHelperGenericDebug() {}

      static std::string printNodeStoreConns(const NodeStoreServers* nodes, std::string headline);
      static std::string printNodeConns(Node& node);

   public:
};

#endif /* MSGHELPERGENERICDEBUG_H_ */
