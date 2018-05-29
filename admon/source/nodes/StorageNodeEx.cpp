#include "StorageNodeEx.h"
#include <program/Program.h>


#define STORAGENODEDATA_LOGFILEPATH "/var/log/beegfs-storage.log"


StorageNodeEx::StorageNodeEx(std::string nodeID, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList& nicList)
   : Node(nodeID, nodeNumID, portUDP, portTCP, nicList)
{
   initialize();
}

void StorageNodeEx::initialize()
{
   this->db = Program::getApp()->getDB();
   data.time = 0;
   data.diskSpaceTotal = 0;
   data.diskSpaceFree = 0;
   data.isResponding = true;
   data.indirectWorkListSize = 0;
   data.directWorkListSize = 0;
   data.sessionCount = 0;
   data.diskRead = 0;
   data.diskWrite = 0;
   data.diskReadPerSec = 0;
   data.diskWritePerSec = 0;

   // set the maximum size of the oldDataList according to the queryInterval
   // should save exactly one hour
   int queryInterval = Program::getApp()->getConfig()->getQueryInterval();
   this->maxSizeOldData = 3600 / queryInterval;
   this->writtenData.amount = 0;
   this->writtenData.unit = "KiB";
   this->writtenData.divideBy = 1024;
   this->readData.amount = 0;
   this->readData.unit = "KiB";
   this->readData.divideBy = 1024;
   this->netRecv.amount = 0;
   this->netRecv.unit = "KiB";
   this->netRecv.divideBy = 1024;
   this->netSend.amount = 0;
   this->netSend.unit = "KiB";
   this->netSend.divideBy = 1024;

   generalInfo.cpuCount = 0;
   generalInfo.cpuName = "";
   generalInfo.cpuSpeed = 0;
   generalInfo.memFree = 0;
   generalInfo.memTotal = 0;
   generalInfo.logFilePath = STORAGENODEDATA_LOGFILEPATH;
}

void StorageNodeEx::setNotResponding()
{
   SafeMutexLock mutexLock(&mutex);

   this->data.isResponding = false;
   this->data.diskSpaceFree = 0;
   this->data.diskSpaceTotal = 0;
   this->data.directWorkListSize = 0;
   this->data.indirectWorkListSize = 0;
   this->data.sessionCount = 0;

   mutexLock.unlock();

   Program::getApp()->getMailer()->addDownNode(getNumID(), getID(), NODETYPE_Storage);
}

void StorageNodeEx::initializeDBData()
{
   SafeMutexLock mutexLock(&mutex);

   // add the node to the list of storage nodes (only happens if not already
   // in there)
   //  this->db->insertNode(this->getID(),NODETYPE_Storage);
   // create tables for the data
   // this->db->createStorageNodeTables(this->getID());

   TimeAbs t;
   t.setToNow();
   uint64_t now = t.getTimeval()->tv_sec;

   // try to load old data from db
   db->getStorageNodeSets(this->getNumID(), TABTYPE_Normal, now - 3600, now, &(oldData.data));
   db->getStorageNodeSets(this->getNumID(), TABTYPE_Hourly, now - 86400, now, &(hourlyData.data));

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

   mutexLock.unlock();
}

void StorageNodeEx::average(std::list<StorageNodeDataContent> *originalData,
   std::list<StorageNodeDataContent> *outList)
{
   TimeAbs t;
   t.setToNow();

   StorageNodeDataContent content;
   content.time = t.getTimeval()->tv_sec; // seconds since the epoch
   content.isResponding = 1; //fixed!

   unsigned aggregatedIndirectWorkListSize = 0;
   unsigned aggregatedDirectWorkListSize = 0;
   int64_t aggregatedDiskSpaceTotal = 0;
   int64_t aggregatedDiskSpaceFree = 0;
   int64_t aggregatedRead = 0;
   int64_t aggregatedWrite = 0;
   int64_t aggregatedReadPerSec = 0;
   int64_t aggregatedWritePerSec = 0;

   for (std::list<StorageNodeDataContent>::iterator iter = originalData->begin();
      iter != originalData->end(); iter++)
   {
      StorageNodeDataContent old = *iter;
      aggregatedIndirectWorkListSize += old.indirectWorkListSize;
      aggregatedDirectWorkListSize += old.directWorkListSize;
      aggregatedDiskSpaceTotal += old.diskSpaceTotal;
      aggregatedDiskSpaceFree += old.diskSpaceFree;
      aggregatedRead += old.diskRead;
      aggregatedWrite += old.diskWrite;
      aggregatedReadPerSec += old.diskReadPerSec;
      aggregatedWritePerSec += old.diskWritePerSec;

   }

   int size = originalData->size();

   if(size > 0)
   {
      content.directWorkListSize = (int64_t) aggregatedDirectWorkListSize / size;
      content.indirectWorkListSize = (int64_t) aggregatedIndirectWorkListSize / size;
      content.diskSpaceFree = (int64_t) aggregatedDiskSpaceFree / size;
      content.diskSpaceTotal = (int64_t) aggregatedDiskSpaceTotal / size;
      content.diskRead = aggregatedRead;
      content.diskWrite = aggregatedWrite;
      content.diskReadPerSec = (int64_t) aggregatedReadPerSec / size;
      content.diskWritePerSec = (int64_t) aggregatedWritePerSec / size;
      content.sessionCount = 0;
   }
   else
   {
      content.directWorkListSize = 0;
      content.indirectWorkListSize = 0;
      content.diskSpaceFree = 0;
      content.diskSpaceTotal = 0;
      content.diskRead = 0;
      content.diskWrite = 0;
      content.diskReadPerSec = 0;
      content.diskWritePerSec = 0;
      content.sessionCount = 0;
   }

   outList->push_front(content);
}

void StorageNodeEx::update(StorageNodeEx *newNode)
{
   SafeMutexLock mutexLock(&mutex);

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

   StorageNodeDataContent content = newNode->getContent();
   this->data.indirectWorkListSize = content.indirectWorkListSize;
   this->data.directWorkListSize = content.directWorkListSize;
   this->data.diskSpaceFree = content.diskSpaceFree;
   this->data.diskSpaceTotal = content.diskSpaceTotal;
   this->data.sessionCount = content.sessionCount;
   this->data.isResponding = true;
   this->data.storageTargets = content.storageTargets;
   this->updateLastHeartbeatTUnlocked();

   TimeAbs t;
   this->data.time = t.getTimeval()->tv_sec;

   //   add high res stats
   this->addHighResStatsList(newNode->getHighResData());
   // save data
   this->oldData.data.push_front(data);

   // write it to DB
   this->db->insertStorageNodeData(this->getID(), this-> getNumID(), data);

   if (this->data.time > this->hourlyData.nextUpdate)
   {
      // take the average data of every field and save it with time of now
      average(&(this->oldData.data), &(this->hourlyData.data));

      this->hourlyData.nextUpdate += 3600;

      // average pushed the new entry to the front of the list
      // take this value and write to db
      this->db->insertStorageNodeData(this->getID(), this-> getNumID(), TABTYPE_Hourly,
         this->hourlyData.data.front());
   }

   if (this->data.time > this->dailyData.nextUpdate)
   {
      average(&(this->hourlyData.data), &(this->dailyData.data));

      this->dailyData.nextUpdate += 86400;

      // average pushed the new entry to the front of the list
      // take this value and write to db
      this->db->insertStorageNodeData(this->getID(), this-> getNumID(), TABTYPE_Daily,
         this->dailyData.data.front());
   }

   mutexLock.unlock();
}

void StorageNodeEx::addHighResStatsList(HighResStatsList stats)
{
   double readPerSecSum = 0;
   double writePerSecSum = 0;

   for (HighResStatsListRevIter iter = stats.rbegin(); iter != stats.rend();
      ++iter)
   {
      HighResolutionStats s = *iter;
      this->highResStats.push_back(s);
      readPerSecSum += (double) (s.incVals.diskReadBytes) / 1024 / 1024;
      writePerSecSum += (double) (s.incVals.diskWriteBytes) / 1024 / 1024;

      // add the read/written bytes to the DataCounts and change according to
      // unit
      this->readData.amount += (double) (s.incVals.diskReadBytes) /
         this->readData.divideBy;
      this->writtenData.amount += (double) (s.incVals.diskWriteBytes) /
         this->writtenData.divideBy;

      if (this->readData.amount > 1024)
      {
         if (this->readData.unit == "KiB")
         {
            this->readData.divideBy = this->readData.divideBy * 1024;
            this->readData.unit = "MiB";
            this->readData.amount = this->readData.amount / 1024;
         }
         else
         if (this->readData.unit == "MiB")
         {
            this->readData.divideBy = this->readData.divideBy * 1024;
            this->readData.unit = "GiB";
            this->readData.amount = this->readData.amount / 1024;
         }
         else
         if (this->readData.unit == "GiB")
         {
            this->readData.divideBy = this->readData.divideBy * 1024;
            this->readData.unit = "TiB";
            this->readData.amount = this->readData.amount / 1024;
         }
      }

      if (this->writtenData.amount > 1024)
      {
         if (this->writtenData.unit == "KiB")
         {
            this->writtenData.divideBy = this->writtenData.divideBy * 1024;
            this->writtenData.unit = "MiB";
            this->writtenData.amount = this->writtenData.amount / 1024;
         }
         else
         if (this->writtenData.unit == "MiB")
         {
            this->writtenData.divideBy = this->writtenData.divideBy * 1024;
            this->writtenData.unit = "GiB";
            this->writtenData.amount = this->writtenData.amount / 1024;
         }
         else
         if (this->writtenData.unit == "GiB")
         {
            this->writtenData.divideBy = this->writtenData.divideBy * 1024;
            this->writtenData.unit = "TiB";
            this->writtenData.amount = this->writtenData.amount / 1024;
         }
      }

      // add the reveiced/sent bytes to the DataCounts and change according to
      // unit
      this->netRecv.amount += (double) (s.incVals.netRecvBytes) /
         this->netRecv.divideBy;
      if (this->netRecv.amount > 1024)
      {
         if (this->netRecv.unit == "KiB")
         {
            this->netRecv.divideBy = this->netRecv.divideBy * 1024;
            this->netRecv.unit = "MiB";
            this->netRecv.amount = this->netRecv.amount / 1024;
         }
         else
         if (this->netRecv.unit == "MiB")
         {
            this->netRecv.divideBy = this->netRecv.divideBy * 1024;
            this->netRecv.unit = "GiB";
            this->netRecv.amount = this->netRecv.amount / 1024;
         }
         else
         if (this->netRecv.unit == "GiB")
         {
            this->netRecv.divideBy = this->netRecv.divideBy * 1024;
            this->netRecv.unit = "TiB";
            this->netRecv.amount = this->netRecv.amount / 1024;
         }
      }

      this->netSend.amount += (double) (s.incVals.netSendBytes) /
         this->netSend.divideBy;
      if (this->netSend.amount > 1024)
      {
         if (this->netSend.unit == "KiB")
         {
            this->netSend.divideBy = this->netSend.divideBy * 1024;
            this->netSend.unit = "MiB";
            this->netSend.amount = this->netSend.amount / 1024;
         }
         else
         if (this->netSend.unit == "MiB")
         {
            this->netSend.divideBy = this->netSend.divideBy * 1024;
            this->netSend.unit = "GiB";
            this->netSend.amount = this->netSend.amount / 1024;
         }
         else
         if (this->netSend.unit == "GiB")
         {
            this->netSend.divideBy = this->netSend.divideBy * 1024;
            this->netSend.unit = "TiB";
            this->netSend.amount = this->netSend.amount / 1024;
         }
      }
   }

   this->data.diskRead = (uint64_t) readPerSecSum;
   this->data.diskWrite = (uint64_t) writePerSecSum;
   // calculate a average of disk Performance for this measurement
   if (stats.size() != 0)
   {
      this->data.diskReadPerSec = (int64_t) readPerSecSum / stats.size();
      this->data.diskWritePerSec = (int64_t) writePerSecSum / stats.size();
   }

   // only hold about 10 minutes
   while (this->highResStats.size() > 600)
   {
      this->highResStats.pop_front();
   }
}

void StorageNodeEx::upAgain()
{
   Program::getApp()->getWorkQueue()->addIndirectWork(new GetNodeInfoWork(
      this->getNumID(), NODETYPE_Storage));
}
