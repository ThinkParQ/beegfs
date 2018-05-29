#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/UnitTk.h>
#include <components/Database.h>
#include <program/Program.h>
#include <toolkit/stats/statshelper.h>
#include "NodeStoreStorageEx.h"



NodeStoreStorageEx::NodeStoreStorageEx():
   NodeStoreServers(NODETYPE_Storage, false)
{
   // nothing to be done here
}

bool NodeStoreStorageEx::addOrUpdateNode(std::shared_ptr<StorageNodeEx> node)
{
   std::string nodeID(node->getID());
   NumNodeID nodeNumID = node->getNumID();

   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   bool precheckRes = addOrUpdateNodePrecheck(*node);
   if(!precheckRes)
      return false;

   SafeMutexLock mutexLock(&mutex); // L O C K

   // check if this is a new node or an update of an existing node

   auto iter = activeNodes.find(nodeNumID);
   if(iter != activeNodes.end() )
   { // node exists already => update
      // update node
      std::static_pointer_cast<StorageNodeEx>(iter->second)->update(node.get());
   }
   else
   { // new node
      node->initializeDBData();
   }

   bool retVal = addOrUpdateNodeUnlocked(std::move(node), NULL);

   mutexLock.unlock(); // U N L O C K

   return retVal;
}

/**
 * this function should only be called by sync nodes, all others use the typed version.
 *
 * it is important here that we only insert the specialized node objects into the store, not the
 * generic "class Node" objects.
 */
bool NodeStoreStorageEx::addOrUpdateNode(std::shared_ptr<Node> node)
{
   LogContext context("add/update node");
   context.logErr("Wrong method used!");
   context.logBacktrace();

   return false;
}

bool NodeStoreStorageEx::addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID)
{
   std::string nodeID(node->getID());
   NumNodeID nodeNumID = node->getNumID();

   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   bool precheckRes = addOrUpdateNodePrecheck(*node);
   if(!precheckRes)
      return false;

   SafeMutexLock mutexLock(&mutex); // L O C K

   // check if this is a new node

   auto iter = activeNodes.find(nodeNumID);
   if(iter == activeNodes.end() )
   { // new node
      // new node which will be kept => do the DB operations
      NicAddressList nicList = node->getNicList();

      node = boost::make_unique<StorageNodeEx>(nodeID, nodeNumID, node->getPortUDP(),
            node->getPortTCP(), nicList);

      static_cast<StorageNodeEx&>(*node).initializeDBData();
   }

   bool retVal = addOrUpdateNodeUnlocked(std::move(node), outNodeNumID);

   mutexLock.unlock(); // U N L O C K

   return retVal;
}

bool NodeStoreStorageEx::statusStorage(NumNodeID nodeID, std::string *outInfo)
{
   bool res = true;
   *outInfo = "Up";
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));

   if (node != NULL)
   {
      if (!node->getContent().isResponding)
      {
         res = false;
         *outInfo = "Down";
      }
   }
   else
   {
      res = false;
      *outInfo = "Node not available!";
   }

   return res;
}

std::string NodeStoreStorageEx::diskSpaceTotalSum(std::string *outUnit)
{
   int64_t space = 0;
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceFirstNode());

   while (node != NULL)
   {
      space += node->getContent().diskSpaceTotal;

      node = std::static_pointer_cast<StorageNodeEx>(referenceNextNode(node));
   }

   return StringTk::doubleToStr(UnitTk::mebibyteToXbyte(space, outUnit));
}

std::string NodeStoreStorageEx::diskSpaceFreeSum(std::string *outUnit)
{
   int64_t space = 0;
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceFirstNode());

   while (node != NULL)
   {
      space += node->getContent().diskSpaceFree;

      node = std::static_pointer_cast<StorageNodeEx>(referenceNextNode(node));
   }

   return StringTk::doubleToStr(UnitTk::mebibyteToXbyte(space, outUnit));
}

std::string NodeStoreStorageEx::diskSpaceUsedSum(std::string *outUnit)
{
   int64_t totalSpace = 0;
   int64_t freeSpace = 0;

   auto node = std::static_pointer_cast<StorageNodeEx>(referenceFirstNode());

   while (node != NULL)
   {
      totalSpace += node->getContent().diskSpaceTotal;
      freeSpace += node->getContent().diskSpaceFree;

      node = std::static_pointer_cast<StorageNodeEx>(referenceNextNode(node));
   }

   int64_t space = totalSpace - freeSpace;

   return StringTk::doubleToStr(UnitTk::mebibyteToXbyte(space, outUnit));
}

void NodeStoreStorageEx::diskPerfRead(NumNodeID nodeID, uint timespanM, UInt64List *outListTime,
   UInt64List *outListReadPerSec, UInt64List *outListAverageTime,
   UInt64List *outListAverageReadPerSec, uint64_t startTimestampMS, uint64_t endTimestampMS)
{
   HighResStatsList data;
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));

   if (node != NULL)
      data = node->getHighResData();

   // timeSpan in minutes, 10 minutes are stored server side, if longer span is
   // requested take averaged values from db
   if ((timespanM <= 10) && (!data.empty()))
   {
      for (HighResStatsListIter iter = data.begin(); iter != data.end(); iter++)
      {
         HighResolutionStats content = *iter;
         uint64_t contentTimeMS = content.rawVals.statsTimeMS;

         if ((contentTimeMS > startTimestampMS) && (contentTimeMS < endTimestampMS) )
         {
            outListTime->push_back(content.rawVals.statsTimeMS);
            double diskReadMiB = UnitTk::byteToMebibyte(content.incVals.diskReadBytes);
            outListReadPerSec->push_back(diskReadMiB);
         }
      }
   }
   else //take db data
   {
      TabType tabType = statshelper::selectTableType(timespanM);

      Database *db = Program::getApp()->getDB();
      StorageNodeDataContentList data;

      db->getStorageNodeSets(nodeID, tabType, (startTimestampMS / 1000), (endTimestampMS / 1000),
         &data);

      for (StorageNodeDataContentListIter iter = data.begin(); iter != data.end(); iter++)
      {
         StorageNodeDataContent content = *iter;
         outListTime->push_back(content.time * 1000);
         outListReadPerSec->push_back(content.diskReadPerSec);
      }
   }

   statshelper::floatingAverage(outListReadPerSec, outListTime, outListAverageReadPerSec,
      outListAverageTime, DISPLAY_FLOATING_AVERAGE_ORDER);
}

void NodeStoreStorageEx::diskPerfWrite(NumNodeID nodeID, uint timespanM, UInt64List *outListTime,
   UInt64List *outListWritePerSec, UInt64List *outListAverageTime,
   UInt64List *outListAverageWritePerSec, uint64_t startTimestampMS, uint64_t endTimestampMS)
{
   HighResStatsList data;
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));
   if (node != NULL)
      data = node->getHighResData();

   // timeSpan in minutes, 10 minutes are stored server side, if longer span is
   // requested take averaged values from db
   if ((timespanM <= 10) && (!data.empty()))
   {
      for (HighResStatsListIter iter = data.begin(); iter != data.end(); iter++)
      {
         HighResolutionStats content = *iter;
         uint64_t contentTimeMS = content.rawVals.statsTimeMS;

         if ((contentTimeMS > startTimestampMS) && (contentTimeMS < endTimestampMS) )
         {
            outListTime->push_back(content.rawVals.statsTimeMS);
            double diskWriteMiB = UnitTk::byteToMebibyte(content.incVals.diskWriteBytes);
            outListWritePerSec->push_back(diskWriteMiB);
         }
      }
   }
   else //take db data
   {
      TabType tabType = statshelper::selectTableType(timespanM);

      Database *db = Program::getApp()->getDB();
      StorageNodeDataContentList data;

      db->getStorageNodeSets(nodeID, tabType, (startTimestampMS / 1000), (endTimestampMS / 1000),
         &data);

      for (StorageNodeDataContentListIter iter = data.begin(); iter != data.end(); iter++)
      {
         StorageNodeDataContent content = *iter;
         outListTime->push_back(content.time * 1000);
         outListWritePerSec->push_back(content.diskWritePerSec);
      }
   }
   // calculate averages, it's important to use same order values here,
   // otherwise the chart will be screwed
   statshelper::floatingAverage(outListWritePerSec, outListTime, outListAverageWritePerSec,
      outListAverageTime, DISPLAY_FLOATING_AVERAGE_ORDER);
}

std::string NodeStoreStorageEx::diskRead(NumNodeID nodeID, uint timespanM, std::string *outUnit)
{
   double sum = 0; // sum in MiB
   if (timespanM <= 10)
   {
      auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));
      if (node != NULL)
      {
         std::vector<HighResStatsList> dataSets;
         HighResStatsList data = node->getHighResData();

         for (HighResStatsListIter iter = data.begin(); iter != data.end(); iter++)
         {
            HighResolutionStats s = *iter;
            sum += UnitTk::byteToMebibyte(s.incVals.diskReadBytes);
         }
      }
   }
   else
   {
      TabType tabType = statshelper::selectTableType(timespanM);

      TimeAbs t;
      t.setToNow();
      long timeNow = (t.getTimeval()->tv_sec);
      uint64_t startTime = t.getTimeMS() - (timespanM * 60 * 1000);

      StorageNodeDataContentList data;

      Database *db = Program::getApp()->getDB();
      db->getStorageNodeSets(nodeID, tabType, (startTime / 1000), timeNow, &data);

      for (StorageNodeDataContentListIter iter = data.begin(); iter != data.end(); iter++)
      {
         StorageNodeDataContent content = *iter;
         sum += content.diskRead; // db stores in MiB;
      }
   }

   return StringTk::doubleToStr(UnitTk::mebibyteToXbyte(sum, outUnit));
}

std::string NodeStoreStorageEx::diskWrite(NumNodeID nodeID, uint timespanM, std::string *outUnit)
{
   double sum = 0; // sum in MiB
   if (timespanM <= 10)
   {
      auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));
      if (node != NULL)
      {
         std::vector<HighResStatsList> dataSets;
         HighResStatsList data = node->getHighResData();

         for (HighResStatsListIter iter = data.begin(); iter != data.end(); iter++)
         {
            HighResolutionStats s = *iter;
            sum += UnitTk::byteToMebibyte(s.incVals.diskWriteBytes);
         }
      }
   }
   else
   {
      TabType tabType = statshelper::selectTableType(timespanM);

      TimeAbs t;
      t.setToNow();
      long timeNow = (t.getTimeval()->tv_sec);
      uint64_t startTime = t.getTimeMS() - (timespanM * 60 * 1000);

      StorageNodeDataContentList data;

      Database *db = Program::getApp()->getDB();
      db->getStorageNodeSets(nodeID, tabType, (startTime / 1000), timeNow, &data);

      for (StorageNodeDataContentListIter iter = data.begin(); iter != data.end(); iter++)
      {
         StorageNodeDataContent content = *iter;
         sum += content.diskWrite; // db stores in MiB;
      }
   }

   return StringTk::doubleToStr(UnitTk::mebibyteToXbyte(sum, outUnit));
}

std::string NodeStoreStorageEx::diskReadSum(uint timespanM, std::string *outUnit)
{
   double sum = 0; // sum in MiB
   if (timespanM <= 10)
   {
      auto node = std::static_pointer_cast<StorageNodeEx>(referenceFirstNode());

      while (node != NULL)
      {
         std::vector<HighResStatsList> dataSets;
         HighResStatsList data = node->getHighResData();

         for (HighResStatsListIter iter = data.begin(); iter != data.end(); iter++)
         {
            HighResolutionStats s = *iter;
            sum += UnitTk::byteToMebibyte(s.incVals.diskReadBytes);
         }
         node = std::static_pointer_cast<StorageNodeEx>(referenceNextNode(node));
      }
   }
   else
   {
      TabType tabType = statshelper::selectTableType(timespanM);

      NumNodeIDList nodes;
      Database *db = Program::getApp()->getDB();
      db->getStorageNodes(&nodes);

      TimeAbs t;
      t.setToNow();
      long timeNow = (t.getTimeval()->tv_sec);
      uint64_t startTime = t.getTimeMS() - (timespanM * 60 * 1000);

      for (NumNodeIDListIter iter = nodes.begin(); iter != nodes.end(); iter++)
      {
         StorageNodeDataContentList data;
         NumNodeID nodeID = *iter;
         db->getStorageNodeSets(nodeID, tabType, (startTime / 1000), timeNow, &data);

         for (StorageNodeDataContentListIter contIter = data.begin(); contIter != data.end();
            contIter++)
         {
            StorageNodeDataContent content = *contIter;
            sum += content.diskRead; // db stores in MiB;
         }
      }
   }

   return StringTk::doubleToStr(UnitTk::mebibyteToXbyte(sum, outUnit));
}

std::string NodeStoreStorageEx::diskWriteSum(uint timespanM, std::string *outUnit)
{
   double sum = 0; // sum in MiB
   if (timespanM <= 10)
   {
      auto node = std::static_pointer_cast<StorageNodeEx>(referenceFirstNode());

      while (node != NULL)
      {
         std::vector<HighResStatsList> dataSets;
         HighResStatsList data = node->getHighResData();

         for (HighResStatsListIter iter = data.begin(); iter != data.end(); iter++)
         {
            HighResolutionStats s = *iter;
            sum += UnitTk::byteToMebibyte(s.incVals.diskWriteBytes);
         }
         node = std::static_pointer_cast<StorageNodeEx>(referenceNextNode(node));
      }
   }
   else
   {
      TabType tabType = statshelper::selectTableType(timespanM);

      NumNodeIDList nodes;
      Database *db = Program::getApp()->getDB();
      db->getStorageNodes(&nodes);

      TimeAbs t;
      t.setToNow();
      long timeNow = (t.getTimeval()->tv_sec);
      uint64_t startTime = t.getTimeMS() - (timespanM * 60 * 1000);

      for (NumNodeIDListIter iter = nodes.begin(); iter != nodes.end(); iter++)
      {
         StorageNodeDataContentList data;
         NumNodeID nodeID = *iter;
         db->getStorageNodeSets(nodeID, tabType, (startTime / 1000), timeNow, &data);

         for (StorageNodeDataContentListIter contIter = data.begin(); contIter != data.end();
            contIter++)
         {
            StorageNodeDataContent content = *contIter;
            sum += content.diskWrite; // db stores in MiB
         }
      }
   }

   return StringTk::doubleToStr(UnitTk::mebibyteToXbyte(sum, outUnit));
}

void NodeStoreStorageEx::diskPerfWriteSum(uint timespanM, UInt64List *outListTime,
   UInt64List *outListWritePerSec, UInt64List *outListAverageTime,
   UInt64List *outListAverageWritePerSec, uint64_t startTimestampMS, uint64_t endTimestampMS)
{
   // get all high res datasets and store them in a vector (result is a vector of lists)

   uint64_t valueTimeRange; // time range before and after the selected time

   // if timespan not greater than 10 minutes take direct data from servers, otherwise DB
   std::vector<HighResStatsList> dataSets;
   if (timespanM <= 10)
   {
      valueTimeRange = 500; // 500 ms before and after the time value

      auto node = std::static_pointer_cast<StorageNodeEx>(referenceFirstNode());

      while (node != NULL)
      {
         HighResStatsList data = node->getHighResData();

         dataSets.push_back(data);
         node = std::static_pointer_cast<StorageNodeEx>(referenceNextNode(node));
      }
   }
   else
   {
      TabType tabType = statshelper::selectTableTypeAndTimeRange(timespanM, &valueTimeRange);

      NumNodeIDList nodes;
      Database *db = Program::getApp()->getDB();
      db->getStorageNodes(&nodes);

      for (NumNodeIDListIter iter = nodes.begin(); iter != nodes.end(); iter++)
      {
         StorageNodeDataContentList data;
         HighResStatsList highResData;

         NumNodeID nodeID = *iter;
         db->getStorageNodeSets(nodeID, tabType, (startTimestampMS / 1000), (endTimestampMS / 1000),
            &data);

         for (StorageNodeDataContentListIter contIter = data.begin(); contIter != data.end();
            contIter++)
         {
            StorageNodeDataContent content = *contIter;
            HighResolutionStats stats;
            stats.rawVals.statsTimeMS = content.time * 1000;
            stats.incVals.diskWriteBytes = UnitTk::mebibyteToByte(content.diskWritePerSec);
            highResData.push_back(stats);
         }

         dataSets.push_back(highResData);
      }
   }

   // now add up the values for each time
   // timestamps will never be exactly the same => take about 1 second or 5 seconds, add
   // them and divide it by the span of the first and the last time

   // do as long until all lists are empty
   while(!dataSets.empty() ) // no lists remaining => stop
   {
      int elementCount = dataSets.size();

      // as long as the 'master list' contains elements
      while(!(dataSets[0].empty() ) )
      {
         bool validTimeFound = false;
         double sum = 0;
         uint64_t masterTime = 0;

         while(!validTimeFound)
         {
            masterTime = dataSets[0].front().rawVals.statsTimeMS;
            if(masterTime < startTimestampMS)
            {
               dataSets[0].pop_front();
               if(!(dataSets[0].empty() ) )
               {
                  masterTime = dataSets[0].front().rawVals.statsTimeMS;
               }
               else
               {
                  LOG(GENERAL, ERR, "Didn't find write statistics in the given time frame. "
                      "Please check if the clocks on all servers are in sync.");
                  break;
               }
            }
            else
            {
               validTimeFound = true;
            }
         }

         if(validTimeFound)
         {
            for (int i = 0; i < elementCount; i++)
            {
               while ((!dataSets[i].empty()) &&
                  (dataSets[i].front().rawVals.statsTimeMS < masterTime + valueTimeRange))
               {
                  // add it to sum and pop the element
                  if (dataSets[i].front().rawVals.statsTimeMS >= masterTime - valueTimeRange)
                  {
                     sum += UnitTk::byteToMebibyte(dataSets[i].front().incVals.diskWriteBytes);
                  }
                  dataSets[i].pop_front();
               }
            }
            // add the sum to the out lists (with timestamp of the master list)
            outListTime->push_back(masterTime);
            outListWritePerSec->push_back(sum);
         }
      }
      // the master list was empty => pop it => next loop step will take the
      // next list as master
      dataSets.erase(dataSets.begin());
   }

   // calculate averages, it's important to use same order values here,
   // otherwise the chart will be screwed
   statshelper::floatingAverage(outListWritePerSec, outListTime, outListAverageWritePerSec,
      outListAverageTime, DISPLAY_FLOATING_AVERAGE_ORDER);
}

void NodeStoreStorageEx::diskPerfReadSum(uint timespanM, UInt64List *outListTime,
   UInt64List *outListReadPerSec, UInt64List *outListAverageTime,
   UInt64List *outListAverageReadPerSec, uint64_t startTimestampMS, uint64_t endTimestampMS)
{
   // get all high res datasets and store them in a vector (result is a vector of lists)

   uint64_t valueTimeRange; // time range before and after the selected time

   // if timespan not greater than 10 minutes take direct data from servers, otherwise DB
   std::vector<HighResStatsList> dataSets;
   if (timespanM <= 10)
   {
      valueTimeRange = 500; // 500 ms before and after the time value

      auto node = std::static_pointer_cast<StorageNodeEx>(referenceFirstNode());

      while (node != NULL)
      {
         HighResStatsList data = node->getHighResData();

         dataSets.push_back(data);
         node = std::static_pointer_cast<StorageNodeEx>(referenceNextNode(node));
      }
   }
   else
   {
      TabType tabType = statshelper::selectTableTypeAndTimeRange(timespanM, &valueTimeRange);

      NumNodeIDList nodes;
      Database *db = Program::getApp()->getDB();
      db->getStorageNodes(&nodes);

      for (NumNodeIDListIter iter = nodes.begin(); iter != nodes.end(); iter++)
      {
         StorageNodeDataContentList data;
         HighResStatsList highResData;

         NumNodeID nodeID = *iter;
         db->getStorageNodeSets(nodeID, tabType, (startTimestampMS / 1000), (endTimestampMS / 1000),
            &data);

         for (StorageNodeDataContentListIter contIter = data.begin(); contIter != data.end();
            contIter++)
         {
            StorageNodeDataContent content = *contIter;
            HighResolutionStats stats;
            stats.rawVals.statsTimeMS = content.time * 1000;
            stats.incVals.diskReadBytes = UnitTk::mebibyteToByte(content.diskReadPerSec);
            highResData.push_back(stats);
         }

         dataSets.push_back(highResData);
      }
   }

   // now add up the values for each time
   // timestamps will never be exactly the same => take about 1 second or 5 seconds, add
   // them and divide it by the span of the first and the last time

   // do as long until all lists are empty
   while(!dataSets.empty() ) // no lists remaining => stop
   {
      int elementCount = dataSets.size();

      // as long as the 'master list' contains elements
      while(!(dataSets[0].empty() ) )
      {
         bool validTimeFound = false;
         double sum = 0;
         uint64_t masterTime = 0;

         while(!validTimeFound)
         {
            masterTime = dataSets[0].front().rawVals.statsTimeMS;
            if(masterTime < startTimestampMS)
            {
               dataSets[0].pop_front();
               if(!(dataSets[0].empty() ) )
               {
                  masterTime = dataSets[0].front().rawVals.statsTimeMS;
               }
               else
               {
                  LOG(GENERAL, ERR, "Didn't find read statistics in the given time frame. "
                      "Please check if the clocks on all servers are in sync.");
                  break;
               }
            }
            else
            {
               validTimeFound = true;
            }
         }

         if(validTimeFound)
         {
            for (int i = 0; i < elementCount; i++)
            {
               while ((!dataSets[i].empty()) &&
                  (dataSets[i].front().rawVals.statsTimeMS < masterTime + valueTimeRange))
               {
                  // add it to sum and pop the element
                  if (dataSets[i].front().rawVals.statsTimeMS >= masterTime - valueTimeRange)
                  {
                     sum += UnitTk::byteToMebibyte(dataSets[i].front().incVals.diskReadBytes);
                  }
                  dataSets[i].pop_front();
               }
            }
            // add the sum to the out lists (with timestamp of the master list)
            outListTime->push_back(masterTime);
            outListReadPerSec->push_back(sum);
         }
      }
      // the master list was empty => pop it => next loop step will take the
      // next list as master
      dataSets.erase(dataSets.begin());
   }

   // calculate averages, it's important to use same order values here,
   // otherwise the chart will be screwed
   statshelper::floatingAverage(outListReadPerSec, outListTime, outListAverageReadPerSec,
      outListAverageTime, DISPLAY_FLOATING_AVERAGE_ORDER);
}

unsigned NodeStoreStorageEx::sessionCountStorage(NumNodeID nodeID)
{
   unsigned res = 0;
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));
   if (node != NULL)
   {
      res = node->getContent().sessionCount;
   }
   return res;
}

bool NodeStoreStorageEx::getGeneralStorageNodeInfo(NumNodeID nodeID, GeneralNodeInfo *outInfo)
{
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));

   if (node != NULL)
   {
      *outInfo = node->getGeneralInformation();
      return true;
   }

   return false;
}

NicAddressList NodeStoreStorageEx::getStorageNodeNicList(NumNodeID nodeID)
{
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));
   NicAddressList nicList;
   if (node != NULL)
   {
      nicList = node->getNicList();
   }
   return nicList;
}

void NodeStoreStorageEx::storageTargetsInfo(NumNodeID nodeID,
   StorageTargetInfoList *outStorageTargets)
{
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID));
   if (node != NULL)
   {
      *outStorageTargets = node->getContent().storageTargets;
   }
}

std::string NodeStoreStorageEx::timeSinceLastMessageStorage(NumNodeID nodeID)
{
   auto node = std::static_pointer_cast<StorageNodeEx>(referenceNode(nodeID) );
   if (node != NULL)
   {
      Time time;
      Time oldTime = node->getLastHeartbeatT();

      return StringTk::intToStr(time.elapsedSinceMS(&oldTime) / 1000) + std::string(" seconds");
   }
   else
   {
      return "Node not available!";
   }
}
