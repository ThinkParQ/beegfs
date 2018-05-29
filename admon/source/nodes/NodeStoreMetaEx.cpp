#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/Random.h>
#include <components/Database.h>
#include <program/Program.h>
#include <toolkit/stats/statshelper.h>
#include "NodeStoreMetaEx.h"


NodeStoreMetaEx::NodeStoreMetaEx():
   NodeStoreServers(NODETYPE_Meta, false)
{
   // nothing to be done here
}

bool NodeStoreMetaEx::addOrUpdateNode(std::shared_ptr<MetaNodeEx> node)
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
      std::static_pointer_cast<MetaNodeEx>(iter->second)->update(node.get());
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
bool NodeStoreMetaEx::addOrUpdateNode(std::shared_ptr<Node> node)
{
   LogContext context("add/update node");
   context.logErr("Wrong method used!");
   context.logBacktrace();

   return false;
}

bool NodeStoreMetaEx::addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID)
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

      node = boost::make_unique<MetaNodeEx>(nodeID, nodeNumID, node->getPortUDP(),
            node->getPortTCP(), nicList);

      static_cast<MetaNodeEx&>(*node).initializeDBData();
   }

   bool retVal = addOrUpdateNodeUnlocked(std::move(node), NULL);

   mutexLock.unlock(); // U N L O C K

   return retVal;
}

std::string NodeStoreMetaEx::isRootMetaNode(NumNodeID nodeID)
{
   if(getRootNodeNumID() == nodeID)
      return "Yes";
   else
      return "No";
}

std::string NodeStoreMetaEx::getRootMetaNode()
{
   auto node = referenceNode(getRootNodeNumID() );
   std::string retval;

   if (node != NULL)
      retval = node->getID();

   return retval;
}

std::string NodeStoreMetaEx::getRootMetaNodeIDWithTypeStr()
{
   auto node = referenceNode(getRootNodeNumID() );
   std::string retval;

   if (node != NULL)
      retval = node->getNodeIDWithTypeStr();

   return retval;
}

std::string NodeStoreMetaEx::getRootMetaNodeAsTypedNodeID()
{
   auto node = referenceNode(getRootNodeNumID() );
   std::string retval;

   if (node != NULL)
      retval = node->getTypedNodeID();

   return retval;
}

bool NodeStoreMetaEx::statusMeta(NumNodeID nodeNumID, std::string *outInfo)
{
   bool res = true;
   *outInfo = "Up";
   auto node = std::static_pointer_cast<MetaNodeEx>(referenceNode(nodeNumID) );

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

std::string NodeStoreMetaEx::timeSinceLastMessageMeta(NumNodeID nodeID)
{
   auto node = std::static_pointer_cast<MetaNodeEx>(referenceNode(nodeID));
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

unsigned NodeStoreMetaEx::sessionCountMeta(NumNodeID nodeID)
{
   unsigned res = 0;
   auto node = std::static_pointer_cast<MetaNodeEx>(referenceNode(nodeID) );
   if (node != NULL)
   {
      res = node->getContent().sessionCount;
   }
   return res;
}

void NodeStoreMetaEx::metaDataRequests(NumNodeID nodeID, uint timespanM, StringList
   *outListTime, StringList *outListQueuedRequests, StringList *outListWorkRequests)
{
   HighResStatsList data;
   auto node = std::static_pointer_cast<MetaNodeEx>(referenceNode(nodeID) );
   if (node != NULL)
   {
      data = node->getHighResData();
   }

   // timeSpan in minutes, 10 minutes are stored server side, if longer span is
   // requested take averaged values from db
   if ((timespanM <= 10) && (!data.empty()))
   {
      for (HighResStatsListIter iter = data.begin(); iter != data.end(); iter++)
      {
         HighResolutionStats content = *iter;
         outListTime->push_back(StringTk::uint64ToStr(content.rawVals.statsTimeMS));
         unsigned queuedRequests = content.rawVals.queuedRequests;
         unsigned workRequests = content.incVals.workRequests;
         outListQueuedRequests->push_back(StringTk::uintToStr(queuedRequests));
         outListWorkRequests->push_back(StringTk::uintToStr(workRequests));
      }
   }
   else //take db data
   {
      TabType tabType = statshelper::selectTableType(timespanM);

      TimeAbs t;
      t.setToNow();
      long timeNow = (t.getTimeval()->tv_sec);
      uint64_t startTime = t.getTimeMS() - (timespanM * 60 * 1000);

      MetaNodeDataContentList data;
      Database *db = Program::getApp()->getDB();
      db->getMetaNodeSets(nodeID, tabType, (startTime / 1000), timeNow, &data);

      for (MetaNodeDataContentListIter iter = data.begin(); iter != data.end(); iter++)
      {
         MetaNodeDataContent content = *iter;
         outListTime->push_back(StringTk::uint64ToStr((content.time) * 1000));
         unsigned queuedRequests = content.queuedRequests;
         unsigned workRequests = content.workRequests;
         outListQueuedRequests->push_back(StringTk::uintToStr(queuedRequests));
         outListWorkRequests->push_back(StringTk::uintToStr(workRequests));
      }
   }
}

void NodeStoreMetaEx::metaDataRequestsSum(uint timespanM, StringList *outListTime,
   StringList *outListQueuedRequests, StringList *outListWorkRequests)
{
   uint64_t valueTimeRange; // time range before and after the selected time

   // get all high res datasets and store them in a vector (result is a vector of lists)

   // if timespan not greater than 10 minutes take direct data from servers, otherwise DB
   std::vector<HighResStatsList> dataSets;
   if (timespanM <= 10)
   {
      valueTimeRange = 500; // 500 ms before and after the time value

      auto node = std::static_pointer_cast<MetaNodeEx>(referenceFirstNode());
      while (node != NULL)
      {
         HighResStatsList data = node->getHighResData();

         dataSets.push_back(data);
         node = std::static_pointer_cast<MetaNodeEx>(referenceNextNode(node));
      }
   }
   else
   {
      TabType tabType = statshelper::selectTableTypeAndTimeRange(timespanM, &valueTimeRange);

      NumNodeIDList nodes;
      Database *db = Program::getApp()->getDB();
      db->getMetaNodes(&nodes);

      TimeAbs t;
      t.setToNow();
      long timeNow = (t.getTimeval()->tv_sec);
      uint64_t startTime = t.getTimeMS() - (timespanM * 60 * 1000);

      for (NumNodeIDListIter iter = nodes.begin(); iter != nodes.end(); iter++)
      {
         MetaNodeDataContentList data;
         NumNodeID nodeID = *iter;
         db->getMetaNodeSets(nodeID, tabType, (startTime / 1000), timeNow, &data);
         HighResStatsList highResData;
         for (MetaNodeDataContentListIter contIter = data.begin(); contIter != data.end();
            contIter++)
         {
            MetaNodeDataContent content = *contIter;
            HighResolutionStats stats;
            stats.rawVals.statsTimeMS = content.time * 1000;
            stats.rawVals.queuedRequests = content.queuedRequests;
            stats.incVals.workRequests = content.workRequests;
            highResData.push_back(stats);
         }
         dataSets.push_back(highResData);
      }
   }

   // do as long until all lists are empty
   while (!dataSets.empty())
   {
      int elementCount = dataSets.size();

      // as long as the 'master list' contains elements
      while (!(dataSets[0].empty()))
      {
         unsigned queuedRequestsSum = 0;
         unsigned workRequestsSum = 0;
         uint64_t masterTime = dataSets[0].front().rawVals.statsTimeMS;

         for (int i = 0; i < elementCount; i++)
         {
            while ((!dataSets[i].empty()) &&
               (dataSets[i].front().rawVals.statsTimeMS < masterTime + valueTimeRange))
            {
               // add it to sum and pop the element
               if (dataSets[i].front().rawVals.statsTimeMS >= masterTime - valueTimeRange)
               {
                  queuedRequestsSum += dataSets[i].front().rawVals.queuedRequests;
                  workRequestsSum += dataSets[i].front().incVals.workRequests;
               }
               dataSets[i].pop_front();
            }
         }
         // add the sum to the out lists (with timestamp of the master list)
         outListTime->push_back(StringTk::uint64ToStr(masterTime));
         outListQueuedRequests->push_back( StringTk::uintToStr(queuedRequestsSum));
         outListWorkRequests->push_back(StringTk::uintToStr(workRequestsSum));
      }
      // the master list was empty => pop it => next loop step will take the
      // next list as master
      dataSets.erase(dataSets.begin());
   }
}

bool NodeStoreMetaEx::getGeneralMetaNodeInfo(NumNodeID nodeID, GeneralNodeInfo *outInfo)
{
   auto node = std::static_pointer_cast<MetaNodeEx>(referenceNode(nodeID));

   if (node != NULL)
   {
      *outInfo = node->getGeneralInformation();
      return true;
   }

   return false;
}

NicAddressList NodeStoreMetaEx::getMetaNodeNicList(NumNodeID nodeID)
{
   auto node = std::static_pointer_cast<MetaNodeEx>(referenceNode(nodeID));
   NicAddressList nicList;
   if (node != NULL)
   {
      nicList = node->getNicList();
   }
   return nicList;
}
