#include "Cassandra.h"

#include <common/storage/StorageTargetInfo.h>
#include <common/toolkit/StringTk.h>
#include <exception/DatabaseException.h>

#include <chrono>
#include <thread>

static const std::string libVersion = "2.9";

template<typename T>
std::function<T> loadSymbol(void* libHandle, const char* name)
{
   dlerror();
   auto f = dlsym(libHandle, name);
   const char* error = dlerror();
   if (error != NULL)
      throw std::runtime_error("Couldn't load symbol: " + std::string(error)
            + "\nThe cassandra plugin requires the datastax client library version " + libVersion
            + ".");
   return reinterpret_cast<T(*)>(f);
}

Cassandra::Cassandra(Config config) :
   cluster(nullptr, [this](CassCluster* c){cluster_free(c);}),
   session(nullptr, [this](CassSession* s){session_free(s);}),
   batch(nullptr, [this](CassBatch* b){batch_free(b);}),
   config(std::move(config)),
   libHandle(nullptr, dlclose),
   numQueries(0)
{
   // Load datastax cassandra library
   dlerror();
   libHandle.reset(dlopen("libcassandra.so", RTLD_NOW));
   const char* error = dlerror();
   if (libHandle == NULL || error != NULL)
   {
      throw std::runtime_error("Couldn't load cassandra client library (libcassandra.so): "
            + std::string(error) + "\nThe cassandra plugin requires the datastax client library"
            + " version " + libVersion + ".");
   }

   // load used symbols
   cluster_new = loadSymbol<decltype(cass_cluster_new)>(
         libHandle.get(), "cass_cluster_new");
   cluster_free = loadSymbol<decltype(cass_cluster_free)>(
         libHandle.get(), "cass_cluster_free");
   session_new = loadSymbol<decltype(cass_session_new)>(
         libHandle.get(), "cass_session_new");
   session_free = loadSymbol<decltype(cass_session_free)>(
         libHandle.get(), "cass_session_free");
   batch_new = loadSymbol<decltype(cass_batch_new)>(
         libHandle.get(), "cass_batch_new");
   batch_free = loadSymbol<decltype(cass_batch_free)>(
         libHandle.get(), "cass_batch_free");
   batch_add_statement = loadSymbol<decltype(cass_batch_add_statement)>(
         libHandle.get(), "cass_batch_add_statement");
   cluster_set_contact_points = loadSymbol<decltype(cass_cluster_set_contact_points)>(
         libHandle.get(), "cass_cluster_set_contact_points");
   cluster_set_port = loadSymbol<decltype(cass_cluster_set_port)>(
         libHandle.get(), "cass_cluster_set_port");
   session_connect = loadSymbol<decltype(cass_session_connect)>(
         libHandle.get(), "cass_session_connect");
   session_execute = loadSymbol<decltype(cass_session_execute)>(
         libHandle.get(), "cass_session_execute");
   session_execute_batch = loadSymbol<decltype(cass_session_execute_batch)>(
         libHandle.get(), "cass_session_execute_batch");
   future_error_code = loadSymbol<decltype(cass_future_error_code)>(
         libHandle.get(), "cass_future_error_code");
   future_error_message = loadSymbol<decltype(cass_future_error_message)>(
         libHandle.get(), "cass_future_error_message");
   future_free = loadSymbol<decltype(cass_future_free)>(
         libHandle.get(), "cass_future_free");
   statement_new = loadSymbol<decltype(cass_statement_new)>(
         libHandle.get(), "cass_statement_new");
   statement_free = loadSymbol<decltype(cass_statement_free)>(
         libHandle.get(), "cass_statement_free");

   cluster.reset(cluster_new());
   session.reset(session_new());
   batch.reset(batch_new(CASS_BATCH_TYPE_LOGGED));

   cluster_set_contact_points(cluster.get(), this->config.host.c_str());
   cluster_set_port(cluster.get(), this->config.port);

   unsigned tries = 0;
   while (true)
   {
      auto connectFuture = std::unique_ptr<CassFuture, decltype(future_free)>(
            session_connect(session.get(), cluster.get()), future_free);

      CassError err = future_error_code(connectFuture.get());
      if (err == CASS_OK)
         break;

      const char* message;
      size_t length;
      future_error_message(connectFuture.get(), &message, &length);

      LOG(DATABASE, ERR, "Couldn't connect to cassandra database: " + std::string(message));
      tries++;
      if (tries >= connectionRetries)
         throw DatabaseException("Connection to cassandra database failed.");
      else
         LOG(DATABASE, WARNING, "Retrying in 10 seconds.");

      std::this_thread::sleep_for(std::chrono::seconds(10));
   }

   // Create and switch to keyspace
   query("CREATE KEYSPACE IF NOT EXISTS " + this->config.database + " WITH "
            + "replication = {'class': 'SimpleStrategy', 'replication_factor' : 3};");
   query("USE " + this->config.database + ";");

   // Create tables
   query("CREATE TABLE IF NOT EXISTS meta ("
         "time timestamp, nodeNumID int, nodeID varchar, isResponding boolean, "
         "indirectWorkListSize int, directWorkListSize int, PRIMARY KEY(time, nodeNumID));");

   query("CREATE TABLE IF NOT EXISTS highResMeta ("
         "time timestamp, nodeNumID int, nodeID varchar, workRequests int, "
         "queuedRequests int, netSendBytes int, netRecvBytes int, PRIMARY KEY(time, nodeNumID));");

   query("CREATE TABLE IF NOT EXISTS storage ("
         "time timestamp, nodeNumID int, nodeID varchar, isResponding boolean, "
         "indirectWorkListSize int, directWorkListSize int, "
         "diskSpaceTotal bigint, diskSpaceFree bigint, PRIMARY KEY(time, nodeNumID));");

   query("CREATE TABLE IF NOT EXISTS highResStorage ("
         "time timestamp, nodeNumID int, nodeID varchar, workRequests int, "
         "queuedRequests int, diskWriteBytes int, diskReadBytes int, "
         "netSendBytes int, netRecvBytes int, PRIMARY KEY(time, nodeNumID));");

   query("CREATE TABLE IF NOT EXISTS storageTargetData ("
         "time timestamp, nodeNumID int, nodeID varchar, storageTargetID int, "
         "diskSpaceTotal bigint, diskSpaceFree bigint, inodesTotal int, inodesFree int, "
         "PRIMARY KEY(time, nodeNumID));");

   query("CREATE TABLE IF NOT EXISTS metaClientOpsByNode ("
         "time timestamp, node varchar, ops map<varchar,int> ,"
         "PRIMARY KEY(time, node));");
   query("CREATE TABLE IF NOT EXISTS storageClientOpsByNode ("
         "time timestamp, node varchar, ops map<varchar,int> ,"
         "PRIMARY KEY(time, node));");
   query("CREATE TABLE IF NOT EXISTS metaClientOpsByUser ("
         "time timestamp, user varchar, ops map<varchar,int> ,"
         "PRIMARY KEY(time, user));");
   query("CREATE TABLE IF NOT EXISTS storageClientOpsByUser ("
         "time timestamp, user varchar, ops map<varchar,int> ,"
         "PRIMARY KEY(time, user));");
}

void Cassandra::query(const std::string& query, bool waitForResult)
{
   CassStatement* statement = statement_new(query.c_str(), 0);
   auto queryFuture = std::unique_ptr<CassFuture, decltype(future_free)>(
         session_execute(session.get(), statement), future_free);
   statement_free(statement);

   if (waitForResult)
   {
      CassError result = future_error_code(queryFuture.get());

      if (result != CASS_OK)
      {
         const char* message;
         size_t length;
         future_error_message(queryFuture.get(), &message, &length);
         throw DatabaseException("Query '" + query + "' failed: " + std::string(message));
      }
   }
}

void Cassandra::insertMetaNodeData(std::shared_ptr<Node> node, const MetaNodeDataContent& data)
{
   std::ostringstream statement;
   statement << "INSERT INTO meta ";
   statement << "(time, nodeNumID, nodeID, isResponding";
   if (data.isResponding)
      statement << ", indirectWorkListSize, directWorkListSize) ";
   else
      statement << ") ";
   statement << "VALUES (";
   statement << "TOTIMESTAMP(NOW()), " << node->getNumID() << ", '" << node->getID() << "', ";
   statement << std::boolalpha << data.isResponding;
   if (data.isResponding)
      statement << ", " << data.indirectWorkListSize << ", " << data.directWorkListSize << ") ";
   else
      statement << ") ";
   statement << "USING TTL " << config.TTLSecs << ";";

   appendQuery(statement.str());
}

void Cassandra::insertStorageNodeData(std::shared_ptr<Node> node,
      const StorageNodeDataContent& data)
{
   std::ostringstream statement;
   statement << "INSERT INTO storage ";
   statement << "(time, nodeNumID, nodeID, isResponding";
   if (data.isResponding)
      statement << ", indirectWorkListSize, directWorkListSize, diskSpaceTotal, diskSpaceFree) ";
   else
      statement << ") ";
   statement << "VALUES (";
   statement << "TOTIMESTAMP(NOW()), " << node->getNumID() << ", '" << node->getID() << "', ";
   statement << std::boolalpha << data.isResponding;
   if (data.isResponding)
      statement << ", " << data.indirectWorkListSize << ", " << data.directWorkListSize << ", "
            << data.diskSpaceTotal << ", " << data.diskSpaceFree << ") ";
   else
      statement << ") ";
   statement << "USING TTL " << config.TTLSecs << ";";

   appendQuery(statement.str());

}

void Cassandra::insertHighResMetaNodeData(std::shared_ptr<Node> node,
      const HighResolutionStats& data)
{
   std::ostringstream statement;
   statement << "INSERT INTO highResMeta ";
   statement << "(time, nodeNumID, nodeID, workRequests, ";
   statement << "queuedRequests, netSendBytes, netRecvBytes) VALUES (";
   statement << data.rawVals.statsTimeMS << ", " << node->getNumID() << ", '" << node->getID() << "', ";
   statement << data.incVals.workRequests << ", " << data.rawVals.queuedRequests << ", ";
   statement << data.incVals.netSendBytes << ", " << data.incVals.netRecvBytes << ") ";
   statement << "USING TTL " << config.TTLSecs << ";";

   appendQuery(statement.str());
}

void Cassandra::insertHighResStorageNodeData(std::shared_ptr<Node> node,
      const HighResolutionStats& data)
{
   std::ostringstream statement;
   statement << "INSERT INTO highResStorage ";
   statement << "(time, nodeNumID, nodeID, workRequests, ";
   statement << "queuedRequests, diskWriteBytes, diskReadBytes, netSendBytes, netRecvBytes) VALUES (";
   statement << data.rawVals.statsTimeMS << ", " << node->getNumID() << ", '" << node->getID() << "', ";
   statement << data.incVals.workRequests << ", " << data.rawVals.queuedRequests << ", ";
   statement << data.incVals.diskWriteBytes << ", " << data.incVals.diskReadBytes << ", ";
   statement << data.incVals.netSendBytes << ", " << data.incVals.netRecvBytes << ") ";
   statement << "USING TTL " << config.TTLSecs << ";";

   appendQuery(statement.str());
}

void Cassandra::insertStorageTargetsData(std::shared_ptr<Node> node,
      const StorageTargetInfo& data)
{
   std::ostringstream statement;
   statement << "INSERT INTO storageTargetData ";
   statement << "(time, nodeNumID, nodeID, storageTargetID, ";
   statement << "diskSpaceTotal, diskSpaceFree, inodesTotal, inodesFree) VALUES (";
   statement << "TOTIMESTAMP(NOW()), " << node->getNumID() << ", '" << node->getID() << "', ";
   statement << data.getTargetID() << ", ";
   statement << data.getDiskSpaceTotal() << ", " << data.getDiskSpaceFree() << ", ";
   statement << data.getInodesTotal() << ", " << data.getInodesFree() << ") ";
   statement << "USING TTL " << config.TTLSecs << ";";

   appendQuery(statement.str());
}

void Cassandra::insertClientNodeData(const std::string& id, const NodeType nodeType,
      const std::map<std::string, uint64_t>& opMap, bool perUser)
{
   std::ostringstream statement;
   statement << "INSERT INTO ";
   if (perUser)
   {
      if (nodeType == NODETYPE_Meta)
         statement << "metaClientOpsByUser";
      else if (nodeType == NODETYPE_Storage)
         statement << "storageClientOpsByUser";
      else
         throw DatabaseException("Invalid Nodetype given.");

      statement << " (time, user, ops) VALUES (";
   }
   else
   {
      if (nodeType == NODETYPE_Meta)
         statement << "metaClientOpsByNode";
      else if (nodeType == NODETYPE_Storage)
         statement << "storageClientOpsByNode";
      else
         throw DatabaseException("Invalid Nodetype given.");

      statement << " (time, node, ops) VALUES (";
   }

   statement << "TOTIMESTAMP(NOW()), '" << id << "', {";

   bool first = true;

   for (auto iter = opMap.begin(); iter != opMap.end(); iter++)
   {
      if (iter->second == 0)
         continue;

      statement << (first ? "" : ",") << "'" << iter->first << "':" << iter->second;
      first = false;
   }

   statement << "}) USING TTL " << config.TTLSecs << ";";

   // if no fields are != 0, dont write anything
   if (!first)
      appendQuery(statement.str());
}

void Cassandra::appendQuery(const std::string& query)
{
   const std::lock_guard<Mutex> lock(queryMutex);

   CassStatement* statement = statement_new(query.c_str(), 0);
   batch_add_statement(batch.get(), statement);
   statement_free(statement);

   numQueries++;

   if (numQueries >= config.maxInsertsPerBatch)
   {
      writeUnlocked();
   }
}

void Cassandra::write()
{
   const std::lock_guard<Mutex> lock(queryMutex);

   if(numQueries)
      writeUnlocked();
}

void Cassandra::writeUnlocked()
{
   CassFuture* batchFuture = session_execute_batch(session.get(), batch.get());
   batch.reset(batch_new(CASS_BATCH_TYPE_LOGGED));
   future_free(batchFuture);

   LOG(DATABASE, DEBUG, "Sent queries to Cassandra.", numQueries);
   numQueries = 0;
}

