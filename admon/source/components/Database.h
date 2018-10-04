#ifndef DATABASE_H_
#define DATABASE_H_


/*
 * Database is a wrapper class around sqlite, that provides common used functionality in a more
 * usable way
 */

#include <common/app/log/LogContext.h>
#include <common/toolkit/TimeAbs.h>
#include <common/Common.h>
#include <exception/DatabaseException.h>
#include <nodes/MetaNodeEx.h>
#include <nodes/StorageNodeEx.h>
#include <toolkit/security/SecurityTk.h>

#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <string>
#include <sstream>
#include <sqlite3.h>
#include <list>

// default passwords for the GUI that are written to DB at first startup
#define DEFAULT_PASS_INFORMATION "information"
#define DEFAULT_PASS_ADMIN "admin"

// number of days to keep values in DB
#define KEEP_NORMAL_DAYS 4;
#define KEEP_HOURLY_DAYS 14;
#define KEEP_DAILY_DAYS 365;

// the ID of the default group fro nodes
#define DEFAULT_GROUP_ID -255

// forward declarations
struct MetaNodeDataContent;
struct StorageNodeDataContent;
class App;

typedef std::list<StorageNodeDataContent> StorageNodeDataContentList;
typedef StorageNodeDataContentList::iterator StorageNodeDataContentListIter;
typedef std::list<MetaNodeDataContent> MetaNodeDataContentList;
typedef MetaNodeDataContentList::iterator MetaNodeDataContentListIter;

/*
 * definition of different table types;
 * for the node stats there are three different types of tables :
 * - TABTYPE_Normal : table with precise values
 * - TABTYPE_Hourly : table with hourly aggregated values
 * - TABTYPE_Daily : table with daily aggregated values
 */
enum TabType
{
   TABTYPE_Normal = 0,
   TABTYPE_Hourly = 1,
   TABTYPE_Daily = 2
};

// contains the index of the table names, use this enum to address the table
// names in in the array tableNames
// keep in sync with the array tableNames and do not change the order of
// the values
enum TableNames
{
   TableNames_users = 0,
   TableNames_groups = 1,
   TableNames_runtimeConfig = 2,
   TableNames_groupmembersMeta = 3,
   TableNames_groupmembersStorage = 4,
   TableNames_storageTargets = 5,
   TableNames_metaNormal = 6,
   TableNames_metaHourly = 7,
   TableNames_metaDaily = 8,
   TableNames_storageNormal = 9,
   TableNames_storageHourly = 10,
   TableNames_storageDaily = 11
};

// contains all table names, access the values by the enum TableNames
// keep in sync with the enum TableNames and do not change the order of
// the values
const std::string tableNames[] =
   {
      "users",                   // TableNames_users = 0
      "groups",                  // TableNames_groups = 1
      "runtimeConfig",           // TableNames_runtimeConfig = 2
      "groupmembersMeta",        // TableNames_groupmembersMeta = 3
      "groupmembersStorage",     // TableNames_groupmembersStorage = 4
      "storageTargets",          // TableNames_storageTargets = 5
      "metaNormal",              // TableNames_metaNormal = 6
      "metaHourly",              // TableNames_metaHourly = 7
      "metaDaily",               // TableNames_metaDaily = 8
      "storageNormal",           // TableNames_storageNormal = 9
      "storageHourly",           // TableNames_storageHourly = 10
      "storageDaily"             // TableNames_storageDaily = 11
   };


class Database
{
   public:
      Database(const std::string& dbFile, bool clearDatabase);
      Database();
      virtual ~Database();

      void createStatisticsTables(NodeType nodeType);
      void insertMetaNodeData(std::string nodeID, NumNodeID nodeNumID, MetaNodeDataContent data);
      void insertMetaNodeData(std::string nodeID, NumNodeID nodeNumID, TabType tabType,
         MetaNodeDataContent data);
      void getMetaNodeSets(NumNodeID nodeNumID, TabType tabType, long minTime, long maxTime,
         MetaNodeDataContentList *outList);
      void insertStorageNodeData(std::string nodeID, NumNodeID nodeNumID,
         StorageNodeDataContent data);
      void insertStorageNodeData(std::string nodeID,  NumNodeID nodeNumID, TabType tabType,
         StorageNodeDataContent data);
      void getStorageNodeSets(NumNodeID nodeNumID, TabType tabType, long minTime, long maxTime,
         StorageNodeDataContentList *outList);
      void getStorageNodes(StringList *outList);
      void getMetaNodes(StringList *outList);
      void getStorageNodes(NumNodeIDList* outList);
      void getMetaNodes(NumNodeIDList* outList);
      void getAllStorageNodeSets(TabType tabType, long minTime, long maxTime,
         std::vector<StorageNodeDataContentList> *outVector);
      std::string getPassword(std::string username);
      void setPassword(std::string username, std::string password);
      void setDisabled(std::string username, bool value);
      bool getDisabled(std::string username);
      void getGroups(StringList *outGroups);
      int addGroup(std::string groupName);
      int addMetaNodeToGroup(std::string nodeID, NumNodeID nodeNumID, int groupID);
      int delGroup(int groupID);
      int delGroup(std::string groupName);
      int addMetaNodeToGroup(std::string nodeID, NumNodeID nodeNumID, std::string groupName);
      int delMetaNodeFromGroup(NumNodeID nodeNumID, int groupID);
      int delMetaNodeFromGroup(NumNodeID nodeNumID, std::string groupName);
      int addStorageNodeToGroup(std::string nodeID, NumNodeID nodeNumID, int groupID);
      int addStorageNodeToGroup(std::string nodeID, NumNodeID nodeNumID, std::string groupName);
      int delStorageNodeFromGroup(NumNodeID nodeNumID, int groupID);
      int delStorageNodeFromGroup(NumNodeID nodeNumID, std::string groupName);
      int getGroupID(std::string groupName);
      std::string getGroupName(int groupID);
      void getMetaNodesInGroup(int groupID, StringList *outNodes);
      void getMetaNodesInGroup(std::string groupName, StringList *outNodes);
      void getMetaNodesInGroup(int groupID, NumNodeIDList *outNodes);
      void getMetaNodesInGroup(std::string groupName, NumNodeIDList *outNodes);
      void getMetaNodesWithoutGroup(StringList *outNodes);
      void getStorageNodesWithoutGroup(StringList *outNodes);
      void getStorageNodesInGroup(int groupID, StringList *outNodes);
      void getStorageNodesInGroup(std::string groupName, StringList *outNodes);
      void getStorageNodesInGroup(int groupID, NumNodeIDList *outNodes);
      void getStorageNodesInGroup(std::string groupName, NumNodeIDList *outNodes);
      void cleanUp();
      bool metaNodeIsInGroup(NumNodeID nodeNumID, int groupID);
      bool metaNodeIsInGroup(NumNodeID nodeNumID, std::string groupName);
      bool storageNodeIsInGroup(NumNodeID nodeNumID, int groupID);
      bool storageNodeIsInGroup(NumNodeID nodeNumID, std::string groupName);
      int metaNodeGetGroup(NumNodeID nodeNumID);
      int storageNodeGetGroup(NumNodeID nodeNumID);
      void readRuntimeConfig();
      int writeRuntimeConfig();


   private:
      std::string dbFile;
      sqlite3 *db;
      LogContext log;
      App *app;
      void initDatabase(bool clearDatabase);
      void WritingQuery(std::string queryStr);
      void createOrCheckDBFile();
      void createInitialTables();
      void createRuntimeConfigTable();
      bool verifyNodeID(std::string nodeID);
      int getNewGroupID(void);
      bool checkUsageOfGroupID(int groupID);
      bool checkAndFixDatabaseCompatibility();
      void addNumNodeIDColumnToTable(std::string tableName);
};

#endif /*DATABASE_H_*/
