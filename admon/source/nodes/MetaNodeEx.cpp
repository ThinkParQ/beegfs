#include "MetaNodeEx.h"
#include <program/Program.h>

#define METANODEDATA_LOGFILEPATH "/var/log/beegfs-meta.log"

MetaNodeEx::MetaNodeEx(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP,
   unsigned short portTCP, NicAddressList& nicList) :
   Node(NODETYPE_Meta, nodeID, nodeNumID, portUDP, portTCP, nicList)
{
   initialize();
}

void MetaNodeEx::initialize()
{
   this->db = Program::getApp()->getDB();
   data.time = 0;
   data.isResponding = true;
   data.indirectWorkListSize = 0;
   data.directWorkListSize = 0;
   data.isRoot = false;

   // set the maximum size of the oldDataList according to the queryInterval
   // should save exactly one hour
   int queryInterval = Program::getApp()->getConfig()->getQueryInterval();
   this->maxSizeOldData = 3600 / queryInterval;
   generalInfo.cpuCount = 0;
   generalInfo.cpuName = "";
   generalInfo.cpuSpeed = 0;
   generalInfo.memFree = 0;
   generalInfo.memTotal = 0;
   generalInfo.logFilePath = METANODEDATA_LOGFILEPATH;
}

void MetaNodeEx::setNotResponding()
{
   {
      const std::lock_guard<Mutex> lock(mutex);
      this->data.isResponding = false;
      this->data.directWorkListSize = 0;
      this->data.indirectWorkListSize = 0;
      this->data.isRoot = false;
      this->data.sessionCount = 0;
   }

   Program::getApp()->getMailer()->addDownNode(getNumID(), getID(), NODETYPE_Meta);
}

void MetaNodeEx::initializeDBData()
{
   const std::lock_guard<Mutex> lock(mutex);

   // create the tables for this node
   // add the node to the list of storage nodes (only happens if not already in there)
   //   this->db->insertNode(this->getID(),NODETYPE_Meta);
   // create tables for the data
   //   this->db->createMetaNodeTables(this->getID());

   TimeAbs t;
   t.setToNow();
   uint64_t now = t.getTimeval()->tv_sec;
   oldData.nextUpdate = 0; //this stays zero because it is not needed here; store is always updated

   // try to load old data from db
   db->getMetaNodeSets(this->getNumID(), TABTYPE_Normal, now - 3600, now, &(oldData.data));
   db->getMetaNodeSets(this->getNumID(), TABTYPE_Hourly, now - 86400, now, &(hourlyData.data));

   // adjust update times
   if (oldData.data.empty())
   {
      hourlyData.nextUpdate = now + 3600;
   }
   else
   {
      uint64_t firstOld = oldData.data.front().time;
      long offset = 3600 - (now - firstOld);
      hourlyData.nextUpdate = now + offset;
   }

   if (hourlyData.data.empty())
   {
      dailyData.nextUpdate = now + 86400;
   }
   else
   {
      uint64_t firstHourly = hourlyData.data.front().time;
      long offset = 86400 - (now - firstHourly);
      dailyData.nextUpdate = now + offset;
   }
}

void MetaNodeEx::average(std::list<MetaNodeDataContent> *originalData,
   std::list<MetaNodeDataContent> *outList)
{
   MetaNodeDataContent content;
   TimeAbs t;
   t.setToNow();
   content.time = t.getTimeval()->tv_sec; // seconds since the epoch
   content.isResponding = true; //fixed!

   unsigned aggregatedIndirectWorkListSize = 0;
   unsigned aggregatedDirectWorkListSize = 0;
   unsigned aggregatedWorkRequests = 0;
   unsigned aggregatedQueuedWorkRequests = 0;

   for(std::list<MetaNodeDataContent>::iterator iter = originalData->begin();
       iter != originalData->end(); iter++)
   {
      MetaNodeDataContent old = *iter;
      aggregatedIndirectWorkListSize += old.indirectWorkListSize;
      aggregatedDirectWorkListSize += old.directWorkListSize;
      aggregatedWorkRequests += old.workRequests;
      aggregatedQueuedWorkRequests += old.queuedRequests;
   }

   size_t size = originalData->size();

   if(size > 0)
   {
      content.directWorkListSize = (unsigned) aggregatedDirectWorkListSize / size;
      content.indirectWorkListSize = (unsigned) aggregatedIndirectWorkListSize / size;
      content.workRequests = (unsigned) aggregatedWorkRequests / size;
      content.queuedRequests = (unsigned) aggregatedQueuedWorkRequests / size;
      content.sessionCount = 0;
   }
   else
   {
      content.directWorkListSize = 0;
      content.indirectWorkListSize = 0;
      content.workRequests = 0;
      content.queuedRequests = 0;
      content.sessionCount = 0;
   }

   outList->push_front(content);
}

void MetaNodeEx::update(MetaNodeEx* newNode)
{
   const std::lock_guard<Mutex> lock(mutex);
   // if old data is too big delete the oldest entry
   if (oldData.data.size() >= maxSizeOldData) //
   {
      this->oldData.data.pop_back();
   }

   if (hourlyData.data.size() >= 24) // max 24 h = 1 day
   {
      this->hourlyData.data.pop_back();
   }

   if (dailyData.data.size() >= 7) // max 7 days
   {
      this->dailyData.data.pop_back();
   }

   MetaNodeDataContent content = newNode->getContent();
   this->data.isRoot = content.isRoot;
   this->data.indirectWorkListSize = content.indirectWorkListSize;
   this->data.directWorkListSize = content.directWorkListSize;
   this->data.sessionCount = content.sessionCount;
   this->updateLastHeartbeatTUnlocked();

   TimeAbs t;
   this->data.time = t.getTimeval()->tv_sec; // this->getLastHeartbeatTUnlocked().getTimeval()->tv_sec;
   this->data.isResponding = true;

   //   add high res stats
   this->addHighResStatsList(newNode->getHighResData());

   // save old data
   this->oldData.data.push_front(data);

   // write it to DB
   this->db->insertMetaNodeData(getID(), getNumID(), data);

   if (this->data.time > this->hourlyData.nextUpdate)
   {
      // take the average data of every field and save it with time of now
      average(&(this->oldData.data), &(this->hourlyData.data));

      this->hourlyData.nextUpdate += 3600;

      // average pushed the new entry to the front of the list
      // take this value and write to db
      this->db->insertMetaNodeData(this->getID(), this-> getNumID(), TABTYPE_Hourly,
         this->hourlyData.data.front());
   }

   if (this->data.time > this->dailyData.nextUpdate)
   {
      average(&(this->hourlyData.data), &(this->dailyData.data));

      this->dailyData.nextUpdate += 86400;

      // average pushed the new entry to the front of the list
      // take this value and write to db
      this->db->insertMetaNodeData(this->getID(), this-> getNumID(), TABTYPE_Daily,
         this->dailyData.data.front());
   }
}

void MetaNodeEx::addHighResStatsList(HighResStatsList stats)
{
   unsigned queuedRequestsSum = 0;
   unsigned workRequestsSum = 0;

   for (HighResStatsListRevIter iter = stats.rbegin(); iter != stats.rend(); ++iter)
   {
      HighResolutionStats s = *iter;
      this->highResStats.push_back(s);
      queuedRequestsSum += s.rawVals.queuedRequests;
      workRequestsSum += s.incVals.workRequests;
   }

   // calculate a average for this measurement
   if (!stats.empty())
   {
      this->data.queuedRequests = (unsigned) (queuedRequestsSum / stats.size());
      this->data.workRequests = (unsigned) (workRequestsSum / stats.size());
   }

   // only hold about 10 minutes
   while (this->highResStats.size() > 600)
   {
      this->highResStats.pop_front();
   }
}

void MetaNodeEx::upAgain()
{
   Program::getApp()->getWorkQueue()->addIndirectWork(new GetNodeInfoWork(this->getNumID(),
      NODETYPE_Meta));
}
