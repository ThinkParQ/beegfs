#include "TSDatabase.h"

#include <common/storage/StorageTargetInfo.h>
#include <common/toolkit/StringTk.h>
#include <exception/DatabaseException.h>
#include <exception/CurlException.h>

#include <boost/algorithm/string/replace.hpp>

static const std::string retentionPolicyName = "auto";

TSDatabase::TSDatabase(Config* config) :
   config(config), numPoints(0)
{
   host = config->getDbHostName();
   port = config->getDbHostPort();
   database = config->getDbDatabase();
   maxPointsPerRequest = config->getDbMaxPointsPerRequest();

   curlWrapper = boost::make_unique<CurlWrapper>(config->getHttpTimeout());

   setupDatabase();
}

void TSDatabase::setupDatabase() const
{
   // Abort startup if InfluxDB is not reachable
   if (!sendPing())
      throw DatabaseException("Can't reach InfluxDB service. Please check your settings and make"
            " sure the InfluxDB daemon is running.");

   // these are called every time the service starts but is being ignored by influxdb if
   // the db and rp already exist
   sendQuery("create database " + database);
   if (config->getDbSetRetentionPolicy())
   {
      sendQuery("create retention policy " + retentionPolicyName + " on " + database
            + " duration " + config->getDbRetentionDuration()
            + " replication 1 default");
   }
}

void TSDatabase::insertMetaNodeData(std::shared_ptr<Node> node, const MetaNodeDataContent& data)
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
   }
   else
   {
      point << " isResponding=" << std::boolalpha << false;
   }

   appendPoint(point.str());
}

void TSDatabase::insertStorageNodeData(std::shared_ptr<Node> node,
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
   }
   else
   {
      point << " isResponding=" << std::boolalpha << false;
   }

   appendPoint(point.str());
}

void TSDatabase::insertHighResMetaNodeData(std::shared_ptr<Node> node,
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

void TSDatabase::insertHighResStorageNodeData(std::shared_ptr<Node> node,
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

void TSDatabase::insertStorageTargetsData(std::shared_ptr<Node> node,
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

void TSDatabase::insertClientNodeData(const std::string& id, const NodeType nodeType,
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


void TSDatabase::appendPoint(const std::string& point)
{
   const std::lock_guard<Mutex> mutexLock(pointsMutex);

   points += point + "\n";
   numPoints++;

   // test also for size? make it an option?
   if (numPoints >= maxPointsPerRequest)
   {
      writePointsUnlocked();
   }
}
void TSDatabase::writePoints()
{
   const std::lock_guard<Mutex> mutexLock(pointsMutex);
   writePointsUnlocked();
}

void TSDatabase::writePointsUnlocked()
{
   sendWrite(points);
   points.clear();
   LOG(DATABASE, DEBUG, "Sent data to InfluxDB.", numPoints);
   numPoints = 0;
}

void TSDatabase::sendWrite(const std::string& data) const
{
   unsigned short responseCode = 0;
   CurlWrapper::ParameterMap params;
   params["db"] = database;

   const std::lock_guard<Mutex> mutexLock(curlMutex);

   try
   {
      responseCode = curlWrapper->sendPostRequest(host + ":" + StringTk::intToStr(port)
         + "/write", data.c_str(), params);
   }
   catch (const CurlException& e)
   {
      LOG(DATABASE, ERR, "Writing to InfluxDB failed due to Curl error.", as("Error", e.what()));
      return;
   }

   if (responseCode < 200 || responseCode >= 300)
   {
      LOG(DATABASE, ERR, "Writing to InfluxDB failed.", responseCode,
            as("responseMessage", curlWrapper->getResponse()));
   }
}

void TSDatabase::sendQuery(const std::string& data) const
{
   unsigned short responseCode = 0;
   CurlWrapper::ParameterMap params;
   params["db"] = database;
   params["q"] = data;

   const std::lock_guard<Mutex> mutexLock(curlMutex);

   try
   {
      responseCode = curlWrapper->sendPostRequest(host + ":" + StringTk::intToStr(port)
         + "/query", "", params);
   }
   catch (const CurlException& e)
   {
      LOG(DATABASE, ERR, "Querying InfluxDB failed due to Curl error.", as("Error", e.what()));
      return;
   }

   if (responseCode < 200 || responseCode >= 300)
   {
      LOG(DATABASE, ERR, "Querying InfluxDB failed.", responseCode,
            as("responseMessage", curlWrapper->getResponse()));
   }
}

bool TSDatabase::sendPing() const
{
   unsigned short responseCode = 0;

   const std::lock_guard<Mutex> mutexLock(curlMutex);

   try
   {
      responseCode = curlWrapper->sendGetRequest(host + ":" + StringTk::intToStr(port) + "/ping",
            CurlWrapper::ParameterMap());
   }
   catch (const CurlException& e)
   {
      LOG(DATABASE, ERR, "Pinging InfluxDB failed due to Curl error.", as("Error", e.what()));
      return false;
   }

   if (responseCode < 200 || responseCode >= 300)
   {
      LOG(DATABASE, ERR, "Pinging InfluxDB failed.", responseCode,
            as("responseMessage", curlWrapper->getResponse()));
      return false;
   }

   return true;
}

/*
 * According to InfluxDB documentation, spaces, "=" and "," need to be escaped for write.
 */
std::string TSDatabase::escapeStringForWrite(const std::string& str)
{
   std::string result = str;
   boost::replace_all(result, " ", "\\ ");
   boost::replace_all(result, "=", "\\=");
   boost::replace_all(result, ",", "\\,");
   return result;
}
