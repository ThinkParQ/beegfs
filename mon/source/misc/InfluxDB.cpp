#include "InfluxDB.h"

#include <common/storage/StorageTargetInfo.h>
#include <common/toolkit/StringTk.h>
#include <exception/DatabaseException.h>
#include <exception/CurlException.h>

#include <thread>
#include <chrono>
#include <boost/algorithm/string/replace.hpp>

static const std::string retentionPolicyName = "auto";

InfluxDB::InfluxDB(Config cfg) :
   config(std::move(cfg))
{
   curlWrapper = boost::make_unique<CurlWrapper>(config.httpTimeout, config.curlCheckSSLCertificates);
   if  (config.dbVersion == INFLUXDB)
   {
      if (!config.username.empty())
         curlWrapper->enableHttpAuth(config.username, config.password);

      setupDatabase();
   }
}

void InfluxDB::setupDatabase() const
{
   // Wait for InfluxDB service being available
   unsigned tries = 0;
   while(!sendPing())
   {
      tries++;
      LOG(DATABASE, ERR, "Coudn't reach InfluxDB service.");
      if (tries >= connectionRetries)
         throw DatabaseException("Connection to InfluxDB failed.");
      else
         LOG(DATABASE, WARNING, "Retrying in 10 seconds.");

      std::this_thread::sleep_for(std::chrono::seconds(10));
   }

   // these are called every time the service starts but is being ignored by influxdb if
   // the db and rp already exist
   sendQuery("create database " + config.database);
   if (config.setRetentionPolicy)
   {
      sendQuery("create retention policy " + retentionPolicyName + " on " + config.database
            + " duration " + config.retentionDuration
            + " replication 1 default");
   }
}

void InfluxDB::insertMetaNodeData(std::shared_ptr<Node> node, const MetaNodeDataContent& data)
{
   std::ostringstream point;
   point << "meta";
   point << ",nodeID=" << escapeStringForWrite(node->getID());
   point << ",nodeNumID=" << node->getNumID();

   if(data.isResponding)
   {
      point << " isResponding=" << std::boolalpha << true;
      point << ",indirectWorkListSize=" << data.indirectWorkListSize;
      point << ",directWorkListSize=" << data.directWorkListSize;
      point << ",hostnameid=\"" << data.hostnameid << "\"";

   }
   else
   {
      point << " isResponding=" << std::boolalpha << false;
   }

   appendPoint(point.str());
}

void InfluxDB::insertStorageNodeData(std::shared_ptr<Node> node,
      const StorageNodeDataContent& data)
{
   std::ostringstream point;
   point << "storage";
   point << ",nodeID=" << escapeStringForWrite(node->getID());
   point << ",nodeNumID=" << node->getNumID();

   if(data.isResponding)
   {
      point << " isResponding=" << std::boolalpha << true;
      point << ",indirectWorkListSize=" << data.indirectWorkListSize;
      point << ",directWorkListSize=" << data.directWorkListSize;
      point << ",diskSpaceTotal=" << data.diskSpaceTotal;
      point << ",diskSpaceFree=" << data.diskSpaceFree;
      point << ",hostnameid=\"" << data.hostnameid << "\"";

   }
   else
   {
      point << " isResponding=" << std::boolalpha << false;
   }

   appendPoint(point.str());
}

void InfluxDB::insertHighResMetaNodeData(std::shared_ptr<Node> node,
      const HighResolutionStats& data)
{
   std::ostringstream point;
   point << "highResMeta";
   point << ",nodeID=" << escapeStringForWrite(node->getID());
   point << ",nodeNumID=" << node->getNumID();

   point << " workRequests=" << data.incVals.workRequests;
   point << ",queuedRequests=" << data.rawVals.queuedRequests;
   point << ",netSendBytes=" << data.incVals.netSendBytes;
   point << ",netRecvBytes=" << data.incVals.netRecvBytes;

   // timestamp in ns
   point << " " << std::chrono::nanoseconds(
         std::chrono::milliseconds(data.rawVals.statsTimeMS)).count();

   appendPoint(point.str());
}

void InfluxDB::insertHighResStorageNodeData(std::shared_ptr<Node> node,
      const HighResolutionStats& data)
{
   std::ostringstream point;
   point << "highResStorage";
   point << ",nodeID=" << escapeStringForWrite(node->getID());
   point << ",nodeNumID=" << node->getNumID();

   point << " workRequests=" << data.incVals.workRequests;
   point << ",queuedRequests=" << data.rawVals.queuedRequests;
   point << ",diskWriteBytes=" << data.incVals.diskWriteBytes;
   point << ",diskReadBytes=" << data.incVals.diskReadBytes;
   point << ",netSendBytes=" << data.incVals.netSendBytes;
   point << ",netRecvBytes=" << data.incVals.netRecvBytes;

   // timestamp in ns
   point << " " << std::chrono::nanoseconds(
         std::chrono::milliseconds(data.rawVals.statsTimeMS)).count();

   appendPoint(point.str());
}

void InfluxDB::insertStorageTargetsData(std::shared_ptr<Node> node,
      const StorageTargetInfo& data)
{
   std::ostringstream point;
   point << "storageTargets";
   point << ",nodeID=" << escapeStringForWrite(node->getID());
   point << ",nodeNumID=" << node->getNumID();
   point << ",storageTargetID=" << data.getTargetID();

   point << " diskSpaceTotal=" << data.getDiskSpaceTotal();
   point << ",diskSpaceFree=" << data.getDiskSpaceFree();
   point << ",inodesTotal=" << data.getInodesTotal();
   point << ",inodesFree=" << data.getInodesFree();

   std::string t;
   if (data.getState() == TargetConsistencyState::TargetConsistencyState_GOOD)
      t = "GOOD";
   else if (data.getState() == TargetConsistencyState::TargetConsistencyState_NEEDS_RESYNC)
      t = "NEEDS_RESYNC";
   else
      t = "BAD";

   point << ",targetConsistencyState=\"" << t << "\"";

   appendPoint(point.str());
}

void InfluxDB::insertClientNodeData(const std::string& id, const NodeType nodeType,
      const std::map<std::string, uint64_t>& opMap, bool perUser)
{
   std::ostringstream point;
   if (perUser)
   {
      if (nodeType == NODETYPE_Meta)
         point << "metaClientOpsByUser";
      else if (nodeType == NODETYPE_Storage)
         point << "storageClientOpsByUser";
      else
         throw DatabaseException("Invalid Nodetype given.");
   }
   else
   {
      if (nodeType == NODETYPE_Meta)
         point << "metaClientOpsByNode";
      else if (nodeType == NODETYPE_Storage)
         point << "storageClientOpsByNode";
      else
         throw DatabaseException("Invalid Nodetype given.");
   }

   point << (perUser ? ",user=" : ",node=") << id;

   bool first = true;

   for (auto iter = opMap.begin(); iter != opMap.end(); iter++)
   {
      if (iter->second == 0)
         continue;

      point << (first ? " " : ",") << iter->first << "=" << iter->second;
      first = false;
   }

   // if no fields are != 0, dont write anything
   if (!first)
      appendPoint(point.str());
}


void InfluxDB::appendPoint(const std::string& point)
{
   const std::lock_guard<Mutex> mutexLock(pointsMutex);

   points += point + "\n";
   numPoints++;

   // test also for size? make it an option?
   if (numPoints >= config.maxPointsPerRequest)
   {
      writePointsUnlocked();
   }
}
void InfluxDB::write()
{
   const std::lock_guard<Mutex> mutexLock(pointsMutex);
   writePointsUnlocked();
}

void InfluxDB::writePointsUnlocked()
{
   sendWrite(points);
   points.clear();
   LOG(DATABASE, DEBUG, "Sent data to InfluxDB.", numPoints);
   numPoints = 0;
}

void InfluxDB::sendWrite(const std::string& data) const
{
   unsigned short responseCode = 0;
   CurlWrapper::ParameterMap params;
   std::string url;
   std::vector<std::string> headers;
   if (config.dbVersion == INFLUXDB)
   {
      params["db"] = config.database;
      url = config.host + ":" + StringTk::intToStr(config.port) + "/write";
   }
   else
   {
      params["org"] = config.organization;
      params["bucket"] = config.bucket;
      url = config.host + ":" + StringTk::intToStr(config.port) + "/api/v2/write";
      headers.push_back("Authorization: Token " + config.token);
   }
   
   const std::lock_guard<Mutex> mutexLock(curlMutex);

   try
   {
      responseCode = curlWrapper->sendPostRequest(url, data.c_str(), params, headers);
   }
   catch (const CurlException& e)
   {
      LOG(DATABASE, ERR, "Writing to InfluxDB failed due to Curl error.", ("Error", e.what()));
      return;
   }

   if (responseCode < 200 || responseCode >= 300)
   {
      LOG(DATABASE, ERR, "Writing to InfluxDB failed.", responseCode,
            ("responseMessage", curlWrapper->getResponse()));
   }
}

void InfluxDB::sendQuery(const std::string& data) const
{
   unsigned short responseCode = 0;
   CurlWrapper::ParameterMap params;
   params["db"] = config.database;
   params["q"] = data;

   const std::lock_guard<Mutex> mutexLock(curlMutex);

   try
   {
      responseCode = curlWrapper->sendPostRequest(config.host + ":"
            + StringTk::intToStr(config.port)
            + "/query", "", params, {});
   }
   catch (const CurlException& e)
   {
      LOG(DATABASE, ERR, "Querying InfluxDB failed due to Curl error.", ("Error", e.what()));
      return;
   }

   if (responseCode < 200 || responseCode >= 300)
   {
      LOG(DATABASE, ERR, "Querying InfluxDB failed.", responseCode,
            ("responseMessage", curlWrapper->getResponse()));
   }
}

bool InfluxDB::sendPing() const
{
   unsigned short responseCode = 0;

   const std::lock_guard<Mutex> mutexLock(curlMutex);

   try
   {
      responseCode = curlWrapper->sendGetRequest(config.host + ":"
            + StringTk::intToStr(config.port) + "/ping", CurlWrapper::ParameterMap());
   }
   catch (const CurlException& e)
   {
      LOG(DATABASE, ERR, "Pinging InfluxDB failed due to Curl error.", ("Error", e.what()));
      return false;
   }

   if (responseCode < 200 || responseCode >= 300)
   {
      LOG(DATABASE, ERR, "Pinging InfluxDB failed.", responseCode,
            ("responseMessage", curlWrapper->getResponse()));
      return false;
   }

   return true;
}

/*
 * According to InfluxDB documentation, spaces, "=" and "," need to be escaped for write.
 */
std::string InfluxDB::escapeStringForWrite(const std::string& str)
{
   std::string result = str;
   boost::replace_all(result, " ", "\\ ");
   boost::replace_all(result, "=", "\\=");
   boost::replace_all(result, ",", "\\,");
   return result;
}
