#include "Database.h"
#include <program/Program.h>

#include <sys/stat.h>

/*
 * @param clearDatabase whether the database should be cleared (the app will start with a fresh one)
 * normally this param is read from config in constructor, but manual override is used when testing
 */
Database::Database(const std::string& dbFile, bool clearDatabase)
 : dbFile(dbFile),
   log("Database"),
   app(Program::getApp() )
{
   initDatabase(clearDatabase);
}

Database::Database()
{
   this->app = Program::getApp();
   Config *config = app->getConfig();
   this->dbFile = config->getDatabaseFile();

   initDatabase(config->getClearDatabase());
}

Database::~Database()
{
   // close the sqlite connection
   sqlite3_close(this->db);
}

/*
 * @param clearDatabase whether the database should be cleared (the app will start with a fresh one)
 * normally this param is read from config in constructor, but manual override is used when testing
 * @throw DatabaseException
 */
void Database::initDatabase(bool clearDatabase)
{
   if (this->dbFile.empty())
      throw InvalidConfigException("The config argument 'databaseFile' with a empty value is "
         "invalid.");

   // if clear database is set in config, delete the DB file before the beginning
   if (clearDatabase)
   {
      if (remove(this->dbFile.c_str()) != 0)
      {
         if (errno == EACCES || errno == EROFS)
         {
            log.log(Log_DEBUG, "Could not delete database file. Permission denied.");
         }
         else
         if (errno == ENOENT)
         {
            log.log(Log_DEBUG, "Could not delete database file. File not found.");
         }
      }
   }

   // check DB file if it's a valid DB file
   createOrCheckDBFile();

   // create the initial tables (if not existing yet)
   createInitialTables();
}

void Database::createOrCheckDBFile()
{
   // create DB file path if it does not exist
   Path path(this->dbFile);
   bool mkdirRes = StorageTk::createPathOnDisk(path, true);

   if ( !mkdirRes )
      throw DatabaseException(DB_CONNECT_ERROR, "Could not create path to database file");

   // check if DB file exists
   bool dbExists = StorageTk::pathExists(this->dbFile);

   // open DBFile
   if (dbExists)
   { // if exists, open DB file read/write without create flag (prevent file override)
      if (sqlite3_open_v2(this->dbFile.c_str(), &(this->db), SQLITE_OPEN_READWRITE, NULL) !=
            SQLITE_OK)
         throw DatabaseException(DB_CONNECT_ERROR, "Could not open database file.");
   }
   else
   { // if not exists, open DB file read/write with create flag
      if (sqlite3_open_v2(this->dbFile.c_str(), &(this->db),
         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
         throw DatabaseException(DB_CONNECT_ERROR, "Could not open database file.");
   }

   // check if a connection with the DB is possible
   std::string query = std::string("CREATE TABLE 'initTest' "
      "(test1 varchar(30) primary key not null, test2 varchar(50));");
   WritingQuery(query);

   query = std::string("DROP TABLE 'initTest';");
   WritingQuery(query);

}

bool Database::verifyNodeID(std::string nodeID)
{
   bool retVal = true;
   if (nodeID.find(" ") != std::string::npos)
   {
      retVal = false;
   }
   return retVal;
}

/*
 * Creates the DB tables, if they are not existing
 */
void Database::createInitialTables()
{
   log.log(Log_DEBUG, "create table " + tableNames[TableNames_users]);
   std::string query = std::string("CREATE TABLE IF NOT EXISTS '" +
      tableNames[TableNames_users] + "' "
      "(username varchar(30) primary key not null,"
      "password varchar(50), "
      "disabled integer not null default false);");
   WritingQuery(query);

   // try to get the pw for the two permission groups; if not set, set defaults
   if (getPassword("information") == "")
   {
      std::string pw = SecurityTk::DoMD5(DEFAULT_PASS_INFORMATION);
      std::string insertQueryStr = "INSERT INTO '" +
         tableNames[TableNames_users] + "' "
         "VALUES('information','" + pw + "','1');";
      WritingQuery(insertQueryStr);
   }

   if (getPassword("admin") == "")
   {
      std::string pw = SecurityTk::DoMD5(DEFAULT_PASS_ADMIN);
      std::string insertQueryStr = "INSERT INTO '" +
         tableNames[TableNames_users] + "' "
         "VALUES('admin','" + pw + "','0');";
      WritingQuery(insertQueryStr);
   }

   log.log(Log_DEBUG, "create table " + tableNames[TableNames_groups]);
   query = std::string("CREATE TABLE IF NOT EXISTS '" +
      tableNames[TableNames_groups] + "' "
      "(id integer primary key not null, "
      "name varchar(500) not null,"
      "UNIQUE(name));");
   WritingQuery(query);

   log.log(Log_DEBUG, "create table " + tableNames[TableNames_groupmembersMeta]);
   query = std::string("CREATE TABLE IF NOT EXISTS '" +
      tableNames[TableNames_groupmembersMeta] + "' "
      "(groupID integer not null,"
      "nodeID varchar(50) not null, "
      "nodeNumID integer not null,"
      "PRIMARY KEY (groupID,nodeID));");
   WritingQuery(query);

   log.log(Log_DEBUG, "create table " + tableNames[TableNames_groupmembersStorage]);
   query = std::string("CREATE TABLE IF NOT EXISTS '" +
      tableNames[TableNames_groupmembersStorage] + "' "
      "(groupID integer not null, "
      "nodeID varchar(50) not null, "
      "nodeNumID integer not null,"
      "PRIMARY KEY (groupID,nodeID));");
   WritingQuery(query);

   log.log(Log_DEBUG, "create table " + tableNames[TableNames_storageTargets]);
   query = std::string("CREATE TABLE IF NOT EXISTS '" +
      tableNames[TableNames_storageTargets] + "' "
      "(id integer not null,"
      "targetID varchar(50) not null,"
      "nodeID varchar(50) not null, "
      "nodeNumID integer not null,"
      "PRIMARY KEY (id,targetID,nodeID));");
   WritingQuery(query);

   createStatisticsTables(NODETYPE_Meta);
   createStatisticsTables(NODETYPE_Storage);

   checkAndFixDatabaseCompatibility();

   createRuntimeConfigTable();
}

/*
 * Creates a table to save configuration, which can be modified at runtime
 * (e.g. mail notification settings)
 */
void Database::createRuntimeConfigTable()
{
   sqlite3_stmt *stmt;

   log.log(Log_DEBUG, "create table " + tableNames[TableNames_runtimeConfig]);
   std::string query = std::string("CREATE TABLE IF NOT EXISTS '" +
      tableNames[TableNames_runtimeConfig] + "' "
      "(parameter varchar(100) primary key not null, "
      "value varchar(100) not null);");
   WritingQuery(query);

   // load the runtime config saved in the appinto a map
   StringMap runtimeCfg;
   app->getRuntimeConfig()->exportAsMap(&runtimeCfg);

   // for each element in the map write it to DB
   for (StringMapIter iter = runtimeCfg.begin(); iter!= runtimeCfg.end(); iter++)
   {
      std::string param = (*iter).first;
      std::string value = (*iter).second;

      // check if the parameter exists in the runtimeConfig table assemble the query
      log.log(Log_DEBUG, "check " + tableNames[TableNames_runtimeConfig] + " table for parameter "
         + param);
      query = std::string("SELECT COUNT(parameter) FROM " +
         tableNames[TableNames_runtimeConfig] + " WHERE parameter='" + param + "';");

      // pass it to sqlite
      if (sqlite3_prepare_v2(this->db, query.c_str(), -1, &stmt, NULL) != SQLITE_OK)
      {
         sqlite3_finalize(stmt);
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), query.c_str());
      }

      // process result
      int stepResult = sqlite3_step(stmt);
      std::string insertQuery;

      if (stepResult == SQLITE_ROW)
      {
         int count = sqlite3_column_int(stmt, 0);

         if (count == 0)
         { // the parameter doesn't exists ==> insert parameter with value
            log.log(Log_DEBUG, "parameter " + param + " doesn't exists in table " +
               tableNames[TableNames_runtimeConfig]);
            insertQuery = "INSERT INTO " + tableNames[TableNames_runtimeConfig] +
               " VALUES ('" + param + "','" + value + "');";

            try
            {
               WritingQuery(insertQuery);
            }
            catch (DatabaseException &e)
            {
               sqlite3_finalize(stmt);
               log.logErr("Could not create RuntimeConfig table.");
               throw;
            }
         }
         stepResult = sqlite3_step(stmt);
      }

      // if processing stops and return code is not SQLITE_DONE there must be an error
      if (stepResult != SQLITE_DONE)
      {
         sqlite3_finalize(stmt);
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), query.c_str());
      }
      if (sqlite3_finalize(stmt) != SQLITE_OK)
      {
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), query.c_str());
      }
   }
}

/*
 * set new runtime config; values will be taken from app; so the way to change the runtime config is
 * to set the new values in the app and then call this function
 */
int Database::writeRuntimeConfig()
{
   StringMap runtimeCfg;
   app->getRuntimeConfig()->exportAsMap(&runtimeCfg);

   std::string query = "";
   for (StringMapIter iter = runtimeCfg.begin(); iter != runtimeCfg.end(); iter++)
   {
      std::string param = (*iter).first;
      std::string value = (*iter).second;
      query = "UPDATE '" + tableNames[TableNames_runtimeConfig] + "' SET value='" + value +
         "' WHERE parameter='" + param + "';";
      try
      {
         WritingQuery(query);
      }
      catch (DatabaseException &e)
      {
         return -1;
      }
   }
   return 0;
}

void Database::readRuntimeConfig()
{
   sqlite3_stmt *stmt;

   StringMap runtimeCfg;

   std::string query = "SELECT * FROM '" + tableNames[TableNames_runtimeConfig] + "';";

   // do the processing
   if (sqlite3_prepare_v2(this->db, query.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), query.c_str());
   }

   // process every row
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      unsigned char* param = const_cast<unsigned char*> (sqlite3_column_text(stmt, 0));
      unsigned char* value = const_cast<unsigned char*> (sqlite3_column_text(stmt, 1));

      runtimeCfg[(char*) param] = (char*) value;
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), query.c_str());
   }

   // import the contents of the DB into the App's runtime config
   app->getRuntimeConfig()->importAsMap(&runtimeCfg);

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), query.c_str());
   }
}

/*
 * generic function to execute a writing query in sqlite DB (e.g. UPDATE, INSERT, ...)
 * @param queryStr The query to execute (e.g. "INSERT xy ....)
 */
void Database::WritingQuery(std::string queryStr)
{
   sqlite3_stmt *stmt;
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   if (sqlite3_step(stmt) != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * create fresh and empty tables for storage node statistics
 */
void Database::createStatisticsTables(NodeType nodeType)
{
   std::string parameterPart;
   TableNames startTable;
   TableNames lastTable;

   if (nodeType == NODETYPE_Storage)
   {
      // generic part of query for storage server (all tables have the same fields)
      parameterPart = std::string(" "
         "(nodeID varchar(50) not null,"
         "nodeNumID integer not null,"
         "time bigint not null,"
         "is_responding bool not null,"
         "indirectWorkListSize integer not null,"
         "directWorkListSize integer not null,"
         "diskSpaceTotal integer not null,"
         "diskSpaceFree integer not null,"
         "diskRead integer not null,"
         "diskWrite integer not null,"
         "diskReadPerSec integer not null,"
         "diskWritePerSec integer not null,"
         "PRIMARY KEY (nodeID,time));");

      startTable = TableNames_storageNormal;
      lastTable =TableNames_storageDaily;
   }
   else
   if (nodeType == NODETYPE_Meta)
   {
      // generic part of query for metadata server (all tables have the same fields)
      parameterPart = std::string(" "
         "(nodeID varchar(50) not null,"
         "nodeNumID integer not null,"
         "time integer not null,"
         "is_responding bool not null,"
         "indirectWorkListSize integer not null,"
         "directWorkListSize integer not null,"
         "queuedRequests integer not null,"
         "workRequests integer not null,"
         "PRIMARY KEY (nodeID,time));");

      startTable = TableNames_metaNormal;
      lastTable = TableNames_metaDaily;
   }
   else
   {
      throw DatabaseException(DB_QUERY_ERROR, (std::string("Useless NodeType for statistics "
         "tables: " + Node::nodeTypeToStr(nodeType))).c_str());
   }

   // create a table for each aggregation level and some indexes for performance reasons
   for (int tableNameIndex = startTable; tableNameIndex <= lastTable; tableNameIndex++)
   {
      std::string type = tableNames[tableNameIndex];
      std::string query;

      // create a table
      log.log(Log_DEBUG, "create table " + type);
      query = "CREATE TABLE IF NOT EXISTS '" + type + "' " + parameterPart;
      WritingQuery(query);

      // create some indexes
      log.log(Log_DEBUG, "create index idx_" + type + "_nodeID");
      query = "CREATE INDEX IF NOT EXISTS 'idx_" + type + "_nodeID' ON '" + type + "' (nodeID)";
      WritingQuery(query);

      try
      {
         log.log(Log_DEBUG, "create index idx_" + type + "_nodeNumID");
         query = "CREATE INDEX IF NOT EXISTS 'idx_" + type + "_nodeNumID' ON '" + type +
            "' (nodeNumID)";
         WritingQuery(query);
      }
      catch (DatabaseException &e)
      {
         if (!(std::string(e.what()).find("has no column named nodeNumID") != std::string::npos))
            throw;
      }

      log.log(Log_DEBUG, "create index idx_" + type + "_time");
      query = "CREATE INDEX IF NOT EXISTS 'idx_" + type + "_time' ON '" + type + "' (time)";
      WritingQuery(query);
   }
}

/*
 * insert a dataset for a specific meta node into the normal table
 *
 * @param nodeID the ID of the metadata node
 * @data the MetaNodeDataContent to write
 */
void Database::insertMetaNodeData(std::string nodeID, NumNodeID nodeNumID, MetaNodeDataContent data)
{
   insertMetaNodeData(nodeID, nodeNumID, TABTYPE_Normal, data);
}

/*
 * insert a dataset for a specific meta node into a given table
 *
 * @param nodeID the ID of the metadata node
 * @tabType table type (normal,hourly,daily) to write to
 * @data the MetaNodeDataContent to write
 */
void Database::insertMetaNodeData(std::string nodeID, NumNodeID nodeNumID, TabType tabType,
   MetaNodeDataContent data)
{
   // choose the right table
   std::string tab;
   switch (tabType)
   {
      case TABTYPE_Normal:
         tab = tableNames[TableNames_metaNormal];
         break;
      case TABTYPE_Hourly:
         tab = tableNames[TableNames_metaHourly];
         break;
      case TABTYPE_Daily:
         tab = tableNames[TableNames_metaDaily];
         break;
   }

   // check if nodeID is OK (i.e. in this case check for whitespaces in ID)
   if (nodeID.find(" ") == std::string::npos)
   {
      // assemble the INSERT query and write it using WritingQuery
      std::ostringstream query;
      query << "INSERT INTO '" << tab << "' ('nodeID', 'nodeNumID', 'time', 'is_responding', "
         "'indirectWorkListSize', 'directWorkListSize', 'queuedRequests', 'workRequests') "
         "VALUES ('";
      query << nodeID << "','";
      query << nodeNumID << "','";
      query << data.time << "','";
      query << data.isResponding << "','";
      query << data.indirectWorkListSize << "','";
      query << data.directWorkListSize << "','";
      query << data.queuedRequests << "','";
      query << data.workRequests << "');";

      try
      {
         WritingQuery(query.str());
      }
      catch (DatabaseException &e)
      {
         std::string errorMsg = e.what();
         if (errorMsg.find("UNIQUE constraint failed") == std::string::npos)
         {
            log.log(Log_NOTICE, "Database error while inserting meta node data!");
            log.log(Log_SPAM, "Exception text follows: " + errorMsg + ". Query: " +
               query.str());
         }
      }
   }
   else
   {
      log.log(Log_NOTICE, "Errors exist in parameter nodeID!");
   }
}

/*
 * insert a dataset for a specific storage node into the normal table
 *
 * @param nodeID the ID of the storage node
 * @data the StorageNodeDataContent to write
 */
void Database::insertStorageNodeData(std::string nodeID, NumNodeID nodeNumID,
   StorageNodeDataContent data)
{
   insertStorageNodeData(nodeID, nodeNumID, TABTYPE_Normal, data);
}

/*
 * insert a dataset for a specific storage node into the normal table
 *
 * @param nodeID the ID of the storage node
 * @tabType table type (normal,hourly,daily) to write to
 * @data the StorageNodeDataContent to write
 */
void Database::insertStorageNodeData(std::string nodeID, NumNodeID nodeNumID, TabType tabType,
   StorageNodeDataContent data)
{
   // choose the right table
   std::string tab;
   switch (tabType)
   {
      case TABTYPE_Normal:
         tab = tableNames[TableNames_storageNormal];
         break;
      case TABTYPE_Hourly:
         tab = tableNames[TableNames_storageHourly];
         break;
      case TABTYPE_Daily:
         tab = tableNames[TableNames_storageDaily];
         break;
   }

   // check if nodeID is OK (i.e. in this case check for whitespaces in ID)
   if ((nodeID.find(" ") == std::string::npos)) // && (node != NULL) )
   {
      // assemble the INSERT query and write it using WritingQuery
      std::ostringstream nodeQuery;
      nodeQuery << "INSERT INTO '" << tab << "' ('nodeID', 'nodeNumID', 'time', 'is_responding', "
               "'indirectWorkListSize', 'directWorkListSize', 'diskSpaceTotal', 'diskSpaceFree',"
               "'diskRead', 'diskWrite', 'diskReadPerSec', 'diskWritePerSec') VALUES ('";
      nodeQuery << nodeID << "','";
      nodeQuery << nodeNumID << "','";
      nodeQuery << data.time << "','";
      nodeQuery << data.isResponding << "','";
      nodeQuery << data.indirectWorkListSize << "','";
      nodeQuery << data.directWorkListSize << "','";
      nodeQuery << data.diskSpaceTotal << "','";
      nodeQuery << data.diskSpaceFree << "','";
      nodeQuery << data.diskRead << "','";
      nodeQuery << data.diskWrite << "','";
      nodeQuery << data.diskReadPerSec << "','";
      nodeQuery << data.diskWritePerSec << "');";
      try
      {
         WritingQuery(nodeQuery.str());
      }
      catch (DatabaseException &e)
      {
         std::string errorMsg = e.what();
         if (errorMsg.find("UNIQUE constraint failed") == std::string::npos)
         {
            log.log(Log_NOTICE, "Database error while inserting storage node data!");
            log.log(Log_SPAM, "Exception text follows: " + errorMsg + ". Query: " +
               nodeQuery.str());
         }
      }
   }
   else
   {
      log.log(Log_NOTICE, "Errors exist in parameter nodeID!");
   }
}

/*
 * get a list with all storage nodes in DB
 *
 * @outList reference to a StringList to write the output to
 */
void Database::getStorageNodes(StringList *outList)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT DISTINCT nodeID FROM '" +
      tableNames[TableNames_storageNormal] + "';");

   // pass the query to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process the output row by row
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      unsigned char* res = const_cast<unsigned char*> (sqlite3_column_text(stmt, 0));
      outList->push_back((char *) res);
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * get a list with all metadata nodes in DB
 *
 * @outList reference to a StringList to write the output to
 */
void Database::getMetaNodes(StringList *outList)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT DISTINCT nodeID FROM '" +
      tableNames[TableNames_metaNormal] + "';");

   // pass the query to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      unsigned char* res = const_cast<unsigned char*> (sqlite3_column_text(stmt, 0));
      outList->push_back((char *) res);
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * get all stats for a given metadata node for a given timeframe
 *
 * @param nodeNumID the ID of the metadata node
 * @tabType table type (normal,hourly,daily) to read from
 * @minTime the beginnig of the timeframe
 * @maxTime the end of the timeframe
 * @outList reference to a MetaNodeDataContentList to write the output to
 *
 */
void Database::getMetaNodeSets(NumNodeID nodeNumID, TabType tabType, long minTime, long maxTime,
   MetaNodeDataContentList *outList)
{
   // take the right table
   std::string tab;
   sqlite3_stmt *stmt;
   switch (tabType)
   {
      case TABTYPE_Normal:
         tab = tableNames[TableNames_metaNormal];
         break;
      case TABTYPE_Hourly:
         tab = tableNames[TableNames_metaHourly];
         break;
      case TABTYPE_Daily:
         tab = tableNames[TableNames_metaDaily];
         break;
   }

   if (nodeNumID != 0)
   {
      // assemble the query
      std::ostringstream query;
      query << "SELECT time, is_responding, indirectWorkListSize, directWorkListSize, "
         "queuedRequests, workRequests FROM '" << tab << "' WHERE nodeNumID=" << nodeNumID <<
         " AND " << minTime << "<=time AND time<=" << maxTime << ";";
      std::string queryStr = query.str();

      // pass it to sqlite
      if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
      {
         sqlite3_finalize(stmt);
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }

      // process every row and assemble a MetaNodeDataContent object from each row, push the objects
      // to the outgoing list
      int stepResult = sqlite3_step(stmt);
      while (stepResult == SQLITE_ROW)
      {
         MetaNodeDataContent row;
         row.isRoot = false;
         row.time = sqlite3_column_int64(stmt, 0);
         row.isResponding = sqlite3_column_int(stmt, 1);
         row.indirectWorkListSize = sqlite3_column_int(stmt, 2);
         row.directWorkListSize = sqlite3_column_int(stmt, 3);
         row.queuedRequests = sqlite3_column_int(stmt, 4);
         row.workRequests = sqlite3_column_int(stmt, 5);
         row.sessionCount = 0;
         outList->push_back(row);
         stepResult = sqlite3_step(stmt);
      }

      // if processing stops and return code is not SQLITE_DONE there must be an error
      if (stepResult != SQLITE_DONE)
      {
         sqlite3_finalize(stmt);
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }
      if (sqlite3_finalize(stmt) != SQLITE_OK)
      {
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }
   }
   else
   {
      log.log(Log_NOTICE, "Errors exist in parameter nodeID!");
   }
}

/*
 * get all stats for a given storage node for a given timeframe
 *
 * @param nodeNumID the ID of the storage node
 * @tabType table type (normal,hourly,daily) to read from
 * @minTime the beginnig of the timeframe
 * @maxTime the end of the timeframe
 * @outList reference to a StorageNodeDataContentList to write the output to
 */
void Database::getStorageNodeSets(NumNodeID nodeNumID, TabType tabType, long minTime, long maxTime,
   StorageNodeDataContentList *outList)
{
   // take the right table
   std::string tab;
   sqlite3_stmt *stmt;
   switch (tabType)
   {
      case TABTYPE_Normal:
         tab = tableNames[TableNames_storageNormal];
         break;
      case TABTYPE_Hourly:
         tab = tableNames[TableNames_storageHourly];
         break;
      case TABTYPE_Daily:
         tab = tableNames[TableNames_storageDaily];
         break;
   }

   if (nodeNumID != 0)
   {
      // assemble the query
      std::ostringstream query;
      query << "SELECT time, is_responding, indirectWorkListSize, directWorkListSize, "
         "diskSpaceTotal, diskSpaceFree, diskRead, diskWrite, diskReadPerSec, "
         "diskWritePerSec  FROM '" << tab << "' WHERE nodeNumID=" << nodeNumID << " AND "
         << minTime << "<=time AND time<=" << maxTime << ";";
      std::string queryStr = query.str();

      // pass the query to sqlite
      if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
      {
         sqlite3_finalize(stmt);
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }

      // process every row and assemble a MetaNodeDataContent object from each
      // row, push the objects to the outgoing list
      int stepResult = sqlite3_step(stmt);
      while (stepResult == SQLITE_ROW)
      {
         StorageNodeDataContent row;
         row.sessionCount = 0;
         row.time = sqlite3_column_int64(stmt, 0);
         row.isResponding = sqlite3_column_int(stmt, 1);
         row.indirectWorkListSize = sqlite3_column_int(stmt, 2);
         row.directWorkListSize = sqlite3_column_int(stmt, 3);
         row.diskSpaceTotal = sqlite3_column_int(stmt, 4);
         row.diskSpaceFree = sqlite3_column_int(stmt, 5);
         row.diskRead = sqlite3_column_int(stmt, 6);
         row.diskWrite = sqlite3_column_int(stmt, 7);
         row.diskReadPerSec = sqlite3_column_int(stmt, 8);
         row.diskWritePerSec = sqlite3_column_int(stmt, 9);
         outList->push_back(row);
         stepResult = sqlite3_step(stmt);
      }

      // if processing stops and return code is not SQLITE_DONE there must be an error
      if (stepResult != SQLITE_DONE)
      {
         sqlite3_finalize(stmt);
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }
      if (sqlite3_finalize(stmt) != SQLITE_OK)
      {
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }
   }
   else
   {
      log.log(Log_NOTICE, "Errors exist in parameter nodeID!");
   }
}

/*
 * get all stats for a all storage nodes for a given timeframe
 *
 * @tabType table type (normal,hourly,daily) to read from
 * @minTime the beginnig of the timeframe
 * @maxTime the end of the timeframe
 * @outVector reference to a vector of StorageNodeDataContentList objects to
 * write the output to
 *
 */
void Database::getAllStorageNodeSets(TabType tabType, long minTime, long maxTime,
   std::vector<StorageNodeDataContentList> *outVector)
{
   // first get the names of all storage nodes in the DB
   NumNodeIDList storagenodes;
   this->getStorageNodes(&storagenodes);

   // now for each storage node get the appropriate sets and push it into
   // the vector
   for (NumNodeIDListIter iter = storagenodes.begin(); iter != storagenodes.end(); iter++)
   {
      StorageNodeDataContentList dataList;
      this->getStorageNodeSets(*iter, tabType, minTime, maxTime, &dataList);
      outVector->push_back(dataList);
   }
}

std::string Database::getPassword(std::string username)
{
   sqlite3_stmt *stmt;
   std::string pw = "";

   // assemble the query
   std::string queryStr = std::string("SELECT password FROM '" + tableNames[TableNames_users] +
      "' WHERE username='") + username + "';";

   // pass the query to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process the result
   int stepResult = sqlite3_step(stmt);
   if (stepResult == SQLITE_ROW)
   {
      unsigned char* res = const_cast<unsigned char*> (sqlite3_column_text( stmt, 0));
      pw = (char *) res;
   }

   // if processing stops and return code is not SQLITE_OK there must be an error
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   return pw;
}

void Database::setPassword(std::string username, std::string password)
{
   if ((username.find(" ") == std::string::npos) && (password.find(" ") == std::string::npos))
   {
      std::string queryStr = "UPDATE '" + tableNames[TableNames_users] + "' SET password='" +
         password + "' WHERE username='" + username + "';";
      WritingQuery(queryStr);
   }
   else
   {
      log.log(Log_NOTICE, "Passwort contains whitespaces");
   }
}

/*
 * check if a user (meant for information user at the moment) was disabled
 * by admin
 */
bool Database::getDisabled(std::string username)
{
   sqlite3_stmt *stmt;
   bool disabled = false;

   // assemble the query
   std::string queryStr = std::string("SELECT disabled FROM '" + tableNames[TableNames_users] +
      "' WHERE username='") + username + "';";

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process the result
   int stepResult = sqlite3_step(stmt);
   if (stepResult == SQLITE_ROW)
   {
      disabled = (bool) (sqlite3_column_int(stmt, 0));
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   return disabled;
}

/*
 * set the disabled flag for a user
 */
void Database::setDisabled(std::string username, bool value)
{
   std::string val = "0";
   if (value)
   {
      val = "1";
   }
   std::string queryStr = "UPDATE '" + tableNames[TableNames_users] + "' SET disabled='" + val +
      "' WHERE username='" + username + "';";
   WritingQuery(queryStr);
}

/*
 * get all groups from database (although groups are not used in GUI yet)
 */
void Database::getGroups(StringList *outGroups)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT name FROM '" + tableNames[TableNames_groups] +
      "' ORDER BY name ASC;");

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row and push each row in the outList
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      const char* name = (const char*) (sqlite3_column_text(stmt, 0));
      outGroups->push_back(name);
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * Searches the next useful group ID
 */
int Database::getNewGroupID()
{
   int nextGroupID = 0;
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT MAX(id) AS max_group_id FROM '" +
      tableNames[TableNames_runtimeConfig] + "';");

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row and push each row in the outList
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      nextGroupID = sqlite3_column_int(stmt, 0);
      nextGroupID++;
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   while (checkUsageOfGroupID(nextGroupID))
   {
      nextGroupID++;
   }

   return nextGroupID;
}

bool Database::checkUsageOfGroupID(int groupID)
{
   sqlite3_stmt *stmt;

   // check if a group with the groupID exists
   // assemble the query
   std::string queryStr = std::string("SELECT id FROM '" + tableNames[TableNames_runtimeConfig] +
      "' WHERE id=" + StringTk::intToStr(groupID) + ";");

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result
   int stepResult = sqlite3_step(stmt);

   // the groupID is in use
   if (stepResult == SQLITE_ROW)
   {
      sqlite3_finalize(stmt);
      return true;
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }


   // check if the groupID is referenced in the table groupmembersMeta
   // assemble the query
   queryStr = std::string("SELECT groupID FROM '" + tableNames[TableNames_groupmembersMeta] +
      "' WHERE groupID=" + StringTk::intToStr(groupID) + ";");

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result
   stepResult = sqlite3_step(stmt);

   // the groupID is in use
   if (stepResult == SQLITE_ROW)
   {
      sqlite3_finalize(stmt);
      return true;
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }


   // check if the groupID is referenced in the table groupmembersStorage
   // assemble the query
   queryStr = std::string("SELECT groupID FROM '" + tableNames[TableNames_groupmembersStorage] +
      "' WHERE groupID=" + StringTk::intToStr(groupID) + ";");

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result
   stepResult = sqlite3_step(stmt);

   // the groupID is in use
   if (stepResult == SQLITE_ROW)
   {
      sqlite3_finalize(stmt);
      return true;
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   return false;
}

/*
 * add a new group to database (although groups are not used in GUI yet)
 *
 * @param groupName The name of the group to add
 */
int Database::addGroup(std::string groupName)
{
   if (!groupName.empty())
   {
      int id = getNewGroupID();

      // assemble the query and run it
      std::string query = "INSERT INTO '" + tableNames[TableNames_groups] + "' VALUES ('" +
         StringTk::intToStr(id) + "','" + groupName + "');";
      try
      {
         WritingQuery(query);
      }
      catch (DatabaseException &e)
      {
         log.log(Log_NOTICE, "Database error while adding a group!");
         log.log(Log_SPAM, "Exception text follows: " + std::string(e.what()));
         return -1;
      }
   }
   else
   {
      log.log(Log_NOTICE, "Errors exist in parameter groupName!");
      return -1;
   }
   return 0;
}

/*
 * delete a group from database (although groups are not used in GUI yet)
 *
 * @param groupID The ID of the group to delete
 */
int Database::delGroup(int groupID)
{
   if (groupID >= 0)
   {
      // delete every member relationship to a metadata node
      std::string query = "DELETE FROM '" + tableNames[TableNames_groupmembersMeta] +
         "' WHERE groupID='" + StringTk::intToStr(groupID) + "';";
      try
      {
         WritingQuery(query);
      }
      catch (DatabaseException &e)
      {
         log.log(Log_NOTICE, "Database error while deleting a group!");
         log.log(Log_SPAM, "Exception text follows: " + std::string(e.what()));
         return -1;
      }

      // delete every member relationship to a storage node
      query = "DELETE FROM '" + tableNames[TableNames_groupmembersStorage] + "' WHERE groupID='" +
         StringTk::intToStr(groupID) + "';";
      try
      {
         WritingQuery(query);
      }
      catch (DatabaseException &e)
      {
         log.log(Log_NOTICE, "Database error while deleting a group!");
         log.log(Log_SPAM, "Exception text follows: " + std::string(e.what()));
         return -1;
      }

      // delete the group from the main group table
      query = "DELETE FROM '" + tableNames[TableNames_groups] + "' WHERE id='" +
         StringTk::intToStr(groupID) + "';";
      try
      {
         WritingQuery(query);
      }
      catch (DatabaseException &e)
      {
         log.log(Log_NOTICE, "Database error while deleting a group!");
         log.log(Log_SPAM, "Exception text follows: " + std::string(e.what()));
         return -1;
      }
   }

   return 0;
}

/*
 * delete a group from database (although groups are not used in GUI yet)
 *
 * @param groupName The name of the group to delete
 */
int Database::delGroup(std::string groupName)
{
   int groupID = getGroupID(groupName);
   return delGroup(groupID);
}

/*
 * add a metadata node to a group (although groups are not used in GUI yet)
 */
int Database::addMetaNodeToGroup(std::string nodeID, NumNodeID nodeNumID, int groupID)
{
   if (groupID == DEFAULT_GROUP_ID)
   {
      return 0;
   }

   // check if groupID and nodeID are valid
   if ((groupID >= 0) && (nodeNumID != 0))
   {
      std::string query = "INSERT INTO '" + tableNames[TableNames_groupmembersMeta] +
         "' VALUES ('" + StringTk::intToStr(groupID) + "','" + nodeID + "'," +
         nodeNumID.str() + ");";
      try
      {
         WritingQuery(query);
      }
      catch (DatabaseException &e)
      {
         log.log(Log_NOTICE, "Database error while adding a node to a group!");
         log.log(Log_SPAM, "Exception text follows: " + std::string(e.what()));
         return -1;
      }
   }
   else
   {
      log.log(Log_NOTICE, "Errors exist in parameter groupName or nodeID!");
      return -1;
   }
   return 0;
}

/*
 * add a metadata node to a group using the name of the group (although groups
 * are not used in GUI yet)
 */
int Database::addMetaNodeToGroup(std::string nodeID, NumNodeID nodeNumID, std::string groupName)
{
   int groupID = getGroupID(groupName);
   return addMetaNodeToGroup(nodeID, nodeNumID, groupID);
}

/*
 * delete a metadata node from a group (although groups are not used in GUI yet)
 */
int Database::delMetaNodeFromGroup(NumNodeID nodeNumID, int groupID)
{
   if (groupID == DEFAULT_GROUP_ID)
   {
      return 0;
   }
   std::string query = "DELETE FROM '" + tableNames[TableNames_groupmembersMeta] +
      "' WHERE nodeNumID=" + nodeNumID.str() + " AND groupID=" +
      StringTk::intToStr(groupID) + ";";
   try
   {
      WritingQuery(query);
   }
   catch (DatabaseException &e)
   {
      return -1;
   }
   return 0;
}

/*
 * delete a metadata node from a group using the name of the group (although groups are not used in
 * GUI yet)
 */
int Database::delMetaNodeFromGroup(NumNodeID nodeNumID, std::string groupName)
{
   int groupID = getGroupID(groupName);
   return delMetaNodeFromGroup(nodeNumID, groupID);
}

/*
 * add a storage node to a group (although groups are not used in GUI yet)
 */
int Database::addStorageNodeToGroup(std::string nodeID, NumNodeID nodeNumID, int groupID)
{
   if (groupID == DEFAULT_GROUP_ID)
   {
      return 0;
   }
   if ((groupID >= 0) && (nodeNumID != 0))
   {
      std::string query = "INSERT INTO '" + tableNames[TableNames_groupmembersStorage] +
         "' VALUES ('" + StringTk::intToStr(groupID) + "','" + nodeID + "'," +
         nodeNumID.str() + ");";
      try
      {
         WritingQuery(query);
      }
      catch (DatabaseException &e)
      {
         log.log(Log_NOTICE, "Database error while adding a node to a group!");
         log.log(Log_SPAM, "Exception text follows: " + std::string(e.what()));
         return -1;
      }
   }
   else
   {
      log.log(Log_NOTICE, "Errors exist in parameter groupName or nodeID!");
      return -1;
   }
   return 0;
}

/*
 * add a storage node to a group using the name of the group (although groups
 * are not used in GUI yet)
 */
int Database::addStorageNodeToGroup(std::string nodeID, NumNodeID nodeNumID, std::string groupName)
{
   int groupID = getGroupID(groupName);
   return addStorageNodeToGroup(nodeID, nodeNumID, groupID);
}

/*
 * delete a storage node from a group (although groups are not used in GUI yet)
 */
int Database::delStorageNodeFromGroup(NumNodeID nodeNumID, int groupID)
{
   if (groupID == DEFAULT_GROUP_ID)
   {
      return 0;
   }
   std::string query = "DELETE FROM '" + tableNames[TableNames_groupmembersStorage] +
      "' WHERE nodeNumID=" + nodeNumID.str() + " AND groupID=" +
      StringTk::intToStr(groupID) + ";";
   try
   {
      WritingQuery(query);
   }
   catch (DatabaseException &e)
   {
      return -1;
   }
   return 0;
}

/*
 * delete a storage node from a group using the name of the group (although groups are not used in
 * GUI yet)
 */
int Database::delStorageNodeFromGroup(NumNodeID nodeNumID, std::string groupName)
{
   int groupID = getGroupID(groupName);
   return delStorageNodeFromGroup(nodeNumID, groupID);
}

/* get the ID to a given group name */
int Database::getGroupID(std::string groupName)
{
   int groupID = -1;

   if (groupName.compare("Default") == 0)
   {
      return DEFAULT_GROUP_ID;
   }

   // assemble the query
   std::string queryStr = "SELECT id FROM '" + tableNames[TableNames_groups] + "' WHERE name='" +
      groupName + "';";

   // pass the query to sqlite
   sqlite3_stmt *stmt;
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      return -1;
   }

   // process result
   int stepResult = sqlite3_step(stmt);
   if (stepResult == SQLITE_ROW)
   {
      groupID = (sqlite3_column_int(stmt, 0));
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      groupID = -1;
   }

   return groupID;
}

/* get the name to a given group ID */
std::string Database::getGroupName(int groupID)
{
   std::string groupName = "";

   // assemble the query
   std::string queryStr = "SELECT name FROM '" + tableNames[TableNames_groups] +
      "' WHERE id=" + StringTk::intToStr(groupID) + ";";

   // pass the query to sqlite
   sqlite3_stmt *stmt;
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      return "";
   }

   // process result
   int stepResult = sqlite3_step(stmt);
   if (stepResult == SQLITE_ROW)
   {
      groupName = (const char*) (sqlite3_column_text(stmt, 0));
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      groupName = "";
   }

   return groupName;
}

/*
 * get all metadata nodes in a given group
 *
 * @groupID the ID of the group to search
 * @outNodes list to save the found nodes into
 */
void Database::getMetaNodesInGroup(int groupID, StringList *outNodes)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT nodeID FROM '" +
      tableNames[TableNames_runtimeConfig] + "' WHERE groupID=" +
      StringTk::intToStr(groupID) + ";");

   // pass the query to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row and push it into the list
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      const char* name = (const char*) (sqlite3_column_text(stmt, 0));

      outNodes->push_back(name);
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * get all metadata nodes in a given group
 *
 * @groupName the name of the group to search
 * @outNodes list to save the found nodes into
 */
void Database::getMetaNodesInGroup(std::string groupName, StringList *outNodes)
{
   int groupID = getGroupID(groupName);
   return getMetaNodesInGroup(groupID, outNodes);
}

/*
 * get all metadata nodes without a group set
 *
 * @outNodes list to save the found nodes into
 */
void Database::getMetaNodesWithoutGroup(StringList *outNodes)
{
   /*
    * as we only have the grouped nodes saved, but do not save explicitely that a node does not
    * belong to any group we first retrieve a list of nodes for which a group is set, and then
    * iterate through all nodes we have to compare them and find out those without a group
    */
   StringList groupedNodes;
   // assemble the query
   std::string queryStr;
   queryStr = std::string("SELECT nodeID FROM '" + tableNames[TableNames_groupmembersMeta] + "';");

   // pass it to sqlite
   sqlite3_stmt *stmt;
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      const char* name = (const char*) (sqlite3_column_text(stmt, 0));
      groupedNodes.push_back(name);
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   NodeStoreMetaEx *metaNodeStore = Program::getApp()->getMetaNodes();
   auto metaNodes = metaNodeStore->referenceAllNodes();
   for (auto nodeIter = metaNodes.begin(); nodeIter != metaNodes.end(); nodeIter++)
   {
      std::string nodeID = (*nodeIter)->getID();
      bool grouped = false;
      for (StringListIter iterGrouped = groupedNodes.begin(); iterGrouped != groupedNodes.end();
         iterGrouped++)
      {
         if (nodeID.compare(*iterGrouped) == 0)
         {
            grouped = true;
            break;
         }
      }
      if (!grouped)
      {
         outNodes->push_back(nodeID);
      }
   }
}

/*
 * get all storage nodes in a given group
 *
 * @groupID the ID of the group to search
 * @outNodes list to save the found nodes into
 */
void Database::getStorageNodesInGroup(int groupID, StringList *outNodes)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = "SELECT nodeID FROM '" + tableNames[TableNames_groupmembersStorage] +
      "' WHERE groupID=" + StringTk::intToStr(groupID) + ";";

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      const char* name = (const char*) (sqlite3_column_text(stmt, 0));

      outNodes->push_back(name);
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * get all storage nodes in a given group
 *
 * @groupName the name of the group to search
 * @outNodes list to save the found nodes into
 */
void Database::getStorageNodesInGroup(std::string groupName, StringList *outNodes)
{
   int groupID = getGroupID(groupName);
   return getStorageNodesInGroup(groupID, outNodes);
}

/*
 * get all storage nodes without a group set
 *
 * @outNodes list to save the found nodes into
 */
void Database::getStorageNodesWithoutGroup(StringList *outNodes)
{
   /*
    * as we only have the grouped nodes saved, but do not save explicitely that
    * a node does not belong to any group we first retrieve a list of nodes for
    * which a group is set, and then iterate through all nodes we have to
    * compare them and find out those without a group
    */

   StringList groupedNodes;

   // assemble the query
   std::string queryStr;
   queryStr = std::string("SELECT nodeID FROM '" +
      tableNames[TableNames_groupmembersStorage] + "';");

   // pass the query to sqlite
   sqlite3_stmt *stmt;
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      const char* name = (const char*) (sqlite3_column_text(stmt, 0));

      groupedNodes.push_back(name);
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   NodeStoreStorageEx *storageNodeStore = Program::getApp()->getStorageNodes();
   auto storageNodes = storageNodeStore->referenceAllNodes();
   for (auto nodeIter = storageNodes.begin(); nodeIter != storageNodes.end(); nodeIter++)
   {
      std::string nodeID = (*nodeIter)->getID();
      bool grouped = false;
      for (StringListIter iterGrouped = groupedNodes.begin(); iterGrouped != groupedNodes.end();
         iterGrouped++)
      {
         if (nodeID.compare(*iterGrouped) == 0)
         {
            grouped = true;
            break;
         }
      }
      if (!grouped)
      {
         outNodes->push_back(nodeID);
      }
   }
}

/*
 * @param nodeNumID ID of the node to check
 * @param groupID ID of the group to check
 *
 * @return true if nodeID is in groupID, false otherwise
 */
bool Database::metaNodeIsInGroup(NumNodeID nodeNumID, int groupID)
{
   bool retVal = false;
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT nodeID FROM '" +
      tableNames[TableNames_groupmembersMeta] + "' WHERE nodeNumID=" +
      nodeNumID.str() + " AND groupID=" + StringTk::intToStr(groupID) + ";");

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result
   int stepResult = sqlite3_step(stmt);

   if (stepResult == SQLITE_ROW)
   {
      retVal = true;
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   return retVal;
}

/*
 * @param nodeID ID of the node to check
 * @param groupName name of the group to check
 *
 * @return true if nodeID is in groupName, false otherwise
 */
bool Database::metaNodeIsInGroup(NumNodeID nodeNumID, std::string groupName)
{
   return metaNodeIsInGroup(nodeNumID, getGroupID(groupName));
}

/*
 * @param nodeNumID numID of the node to check
 * @param groupID ID of the group to check
 *
 * @return true if nodeID is in groupID, false otherwise
 */
bool Database::storageNodeIsInGroup(NumNodeID nodeNumID, int groupID)
{
   bool retVal = false;
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT groupID FROM '" +
      tableNames[TableNames_groupmembersStorage] + "' WHERE nodeNumID=" +
      nodeNumID.str() + " AND groupID=" + StringTk::intToStr(groupID) + ";");

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result
   int stepResult = sqlite3_step(stmt);
   if (stepResult == SQLITE_ROW)
   {
      retVal = true;
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   return retVal;
}

/*
 * @param nodeNumID numID of the node to check
 * @param groupName name of the group to check
 *
 * @return true if nodeID is in groupName, false otherwise
 */
bool Database::storageNodeIsInGroup(NumNodeID nodeNumID, std::string groupName)
{
   return storageNodeIsInGroup(nodeNumID, getGroupID(groupName));
}

/*
 * get the group a given metadata node
 */
int Database::metaNodeGetGroup(NumNodeID nodeNumID)
{
   /* read group from DB; if none is found return default group */
   int retVal = DEFAULT_GROUP_ID;
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT groupID FROM '" +
      tableNames[TableNames_groupmembersMeta] + "' WHERE nodeNumID=" + nodeNumID.str() + ";");

   // do the processing
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process every row
   int stepResult = sqlite3_step(stmt);
   if (stepResult == SQLITE_ROW)
   {
      retVal = sqlite3_column_int(stmt, 0);
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   return retVal;
}

/*
 * get the group a given storage node
 */
int Database::storageNodeGetGroup(NumNodeID nodeNumID)
{
   /* read group from DB; if none is found return default group */
   int retVal = DEFAULT_GROUP_ID;
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT groupID FROM "
      "'" + tableNames[TableNames_groupmembersStorage] + "' WHERE nodeNumID=" +
      nodeNumID.str() + ";");

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result
   int stepResult = sqlite3_step(stmt);
   if (stepResult == SQLITE_ROW)
   {
      retVal = sqlite3_column_int(stmt, 0);
   }

   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   return retVal;
}

/*
 * deletes old values from DB
 */
void Database::cleanUp()
{
   TimeAbs t;
   t.setToNow();

   static long secondsPerDay = 24 * 60 * 60; // 24 hours * 60 minutes * 60 seconds

   long keepNormalDays = KEEP_NORMAL_DAYS;
   long keepHourlyDays = KEEP_HOURLY_DAYS;
   long keepDailyDays = KEEP_DAILY_DAYS;

   long timeNow = (t.getTimeval()->tv_sec);
   long minTime = 0;

   minTime = timeNow - (keepNormalDays * secondsPerDay);

   std::string query = "DELETE FROM " + tableNames[TableNames_metaNormal] +
      " WHERE time<" + StringTk::intToStr(minTime);
   WritingQuery(query);
   query = "DELETE FROM " + tableNames[TableNames_storageNormal] +
      " WHERE time<" + StringTk::intToStr(minTime);
   WritingQuery(query);

   minTime = timeNow - (keepHourlyDays * secondsPerDay);

   query = "DELETE FROM " + tableNames[TableNames_metaHourly] +
      " WHERE time<" + StringTk::intToStr(minTime);
   WritingQuery(query);
   query = "DELETE FROM " + tableNames[TableNames_storageHourly] +
      " WHERE time<" + StringTk::intToStr(minTime);
   WritingQuery(query);

   minTime = timeNow - (keepDailyDays * secondsPerDay);

   query = "DELETE FROM " + tableNames[TableNames_metaDaily] +
      " WHERE time<" + StringTk::intToStr(minTime);
   WritingQuery(query);
   query = "DELETE FROM " + tableNames[TableNames_storageDaily] +
      " WHERE time<" + StringTk::intToStr(minTime);
   WritingQuery(query);
}

/*
 * get a list with all storage nodes in DB
 *
 * @outList reference to a UInt16List to write the output to
 */
void Database::getStorageNodes(NumNodeIDList* outList)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT DISTINCT nodeNumID FROM '" +
      tableNames[TableNames_storageNormal] + "';");

   // pass the query to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process the output row by row
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      int res = (sqlite3_column_int(stmt, 0));
      outList->push_back(NumNodeID(res) );
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * get a list with all metadata nodes in DB
 *
 * @outList reference to a UInt16List to write the output to
 */
void Database::getMetaNodes(NumNodeIDList* outList)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT DISTINCT nodeNumID FROM '" +
      tableNames[TableNames_metaNormal] + "';");

   // pass the query to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      int res = sqlite3_column_int(stmt, 0);
      outList->push_back(NumNodeID(res) );
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * get all metadata nodes in a given group
 *
 * @groupID the ID of the group to search
 * @outNodes list to save the found nodes into
 */
void Database::getMetaNodesInGroup(int groupID, NumNodeIDList* outNodes)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = std::string("SELECT nodeNumID FROM '" +
      tableNames[TableNames_runtimeConfig] + "' WHERE groupID=" +
      StringTk::intToStr(groupID) + ";");

   // pass the query to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row and push it into the list
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      int res = (sqlite3_column_int(stmt, 0));
      outNodes->push_back(NumNodeID(res) );
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * get all metadata nodes in a given group
 *
 * @groupName the name of the group to search
 * @outNodes list to save the found nodes into
 */
void Database::getMetaNodesInGroup(std::string groupName, NumNodeIDList* outNodes)
{
   int groupID = getGroupID(groupName);
   return getMetaNodesInGroup(groupID, outNodes);
}

/*
 * get all storage nodes in a given group
 *
 * @groupID the ID of the group to search
 * @outNodes list to save the found nodes into
 */
void Database::getStorageNodesInGroup(int groupID, NumNodeIDList* outNodes)
{
   sqlite3_stmt *stmt;

   // assemble the query
   std::string queryStr = "SELECT nodeNumID FROM '" + tableNames[TableNames_groupmembersStorage] +
      "' WHERE groupID=" + StringTk::intToStr(groupID) + ";";

   // pass it to sqlite
   if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }

   // process result row by row
   int stepResult = sqlite3_step(stmt);
   while (stepResult == SQLITE_ROW)
   {
      int res = sqlite3_column_int(stmt, 0);
      outNodes->push_back(NumNodeID(res) );
      stepResult = sqlite3_step(stmt);
   }

   // if processing stops and return code is not SQLITE_DONE there must be an error
   if (stepResult != SQLITE_DONE)
   {
      sqlite3_finalize(stmt);
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
   if (sqlite3_finalize(stmt) != SQLITE_OK)
   {
      throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
   }
}

/*
 * get all storage nodes in a given group
 *
 * @groupName the name of the group to search
 * @outNodes list to save the found nodes into
 */
void Database::getStorageNodesInGroup(std::string groupName, NumNodeIDList* outNodes)
{
   int groupID = getGroupID(groupName);
   return getStorageNodesInGroup(groupID, outNodes);
}

/*
 * converts the database format from release 2011.04 to current release if
 * needed, it ignores missing tables
 * @return false if a error occurred and the database is not compatible
 *
 * Changes:
 * 2011.04 to 2012.08: add column nodeNumID into tables groupmembersMeta,
 *    groupmembersStorage, storageTargets, metaNormal, metaHourly, metaDaily,
 *    storageNormal, storageHourly, storageDaily and delete all rows
 */
bool Database::checkAndFixDatabaseCompatibility()
{
   sqlite3_stmt *stmt;
   bool oldDBFound = true;
   std::string queryStr;
   std::string strNodeNumID("nodeNumID");

   // check nodeNumID in different tables
   for (int i = TableNames_groupmembersMeta; i <= TableNames_storageDaily; i++)
   {
      bool tableExists = false;
      std::string table = tableNames[i];

      // first check if table exists
      // assemble the query
      queryStr = std::string("SELECT name FROM 'sqlite_master' WHERE type='table' AND "
         "name='" + table + "';");

      // pass it to sqlite
      if (sqlite3_prepare_v2(this->db, queryStr.c_str(), -1, &stmt, NULL) != SQLITE_OK)
      {
         sqlite3_finalize(stmt);
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }

      // process result
      int stepResult = sqlite3_step(stmt);

      if (stepResult == SQLITE_ROW)
      {
         tableExists = true;
      }

      // process all results, if not => finalize returns an error
      while (stepResult == SQLITE_ROW)
      {
         stepResult = sqlite3_step(stmt);
      }

      // if processing stops and return code is not SQLITE_DONE there must be an error
      if (stepResult != SQLITE_DONE)
      {
         sqlite3_finalize(stmt);
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }
      if (sqlite3_finalize(stmt) != SQLITE_OK)
      {
         throw DatabaseException(DB_QUERY_ERROR, sqlite3_errmsg(db), queryStr.c_str());
      }

      // if table doesn't exist check the next table
      if (!tableExists)
         continue;

      // if the column nodeNumId not exists add the column to the table, delete all rows, and
      // create index for column nodeNumID
      bool newDBFormat = false;
      queryStr = "ALTER TABLE '" + table + "' ADD COLUMN '" + strNodeNumID +
         "' INTEGER NOT NULL DEFAULT '0';";

      try
      {
         WritingQuery(queryStr);
      }
      catch (DatabaseException &e)
      {
         if (std::string(e.what()).find("duplicate column name: nodeNumID") !=
            std::string::npos)
         {
            newDBFormat = true;
            oldDBFound = false;
         }
         else
         {
            throw;
         }
      }

      if (!newDBFormat)
      {
         log.log(Log_DEBUG, "delete rows " + table);
         queryStr = "DELETE FROM '" + table + "';";
         WritingQuery(queryStr);

         log.log(Log_DEBUG, "create index idx_" + table + "_nodeNumID");
         queryStr = "CREATE INDEX IF NOT EXISTS 'idx_" + table + "_nodeNumID' ON '" + table +
            "' (nodeNumID)";
         WritingQuery(queryStr);
      }
   }

   if (oldDBFound)
   {
      log.log(Log_NOTICE, "Converted the database format to new database schema and delete old "
         "values");
   }

   return true;
}
