#include "ClientOps.h"

#include <common/toolkit/MessagingTk.h>
#include <common/net/message/nodes/GetClientStatsMsg.h>
#include <common/net/message/nodes/GetClientStatsRespMsg.h>

#include <common/nodes/NodeOpStats.h>

#include <mutex>

/**
* Add a list of ops belonging to a specific id (e.g. ip/user) to the store if there isn't one already
* or sum it up with the existing one
*/
bool ClientOps::addOpsList(uint64_t id, const OpsList& opsList)
{
   const std::lock_guard<Mutex> lock(idOpsMapMutex);

   // make sure all idOpsLists have the same size
   if(!oldIdOpsMap.empty())
   {
      if(oldIdOpsMap.begin()->second.size() != opsList.size())
      {
         LOG(GENERAL, ERR, "Tried to add opsList with different size than old stored ones.");
         return false;
      }
   }

   if(!idOpsMap.empty())
   {
      if(idOpsMap.begin()->second.size() != opsList.size())
      {
         LOG(GENERAL, ERR, "Tried to add opsList with different size than already stored ones.");
         return false;
      }
   }

   sumOpsListValues(sumOpsList, opsList);
   sumOpsListValues(idOpsMap[id], opsList);

   return true;
}

/**
* Sum up two lists of ops.
*/
void ClientOps::sumOpsListValues(OpsList& list1, const OpsList& list2) const
{
   // Also initializes list1 to zero if it doesn't exist yet
   list1.resize(list2.size());

   std::transform(list1.begin(), list1.end(), list2.begin(), list1.begin(), sum);
}


/**
* Return the difference of all summed up op lists, mapped to their belonging id, between
* the new and the old data (before clear() was called).
*/
ClientOps::IdOpsMap ClientOps::getDiffOpsMap() const
{
   IdOpsMap diffIdOpsMap;

   // if there isn't an old map, just return empty
   if (oldIdOpsMap.empty())
      return diffIdOpsMap;

   for (auto opsMapIter = idOpsMap.begin(); opsMapIter != idOpsMap.end(); opsMapIter++)
   {
      const auto oldOpsMapIter = oldIdOpsMap.find(opsMapIter->first);

      if (oldOpsMapIter == oldIdOpsMap.end())
         continue;

      ASSERT(opsMapIter->second.size() == oldOpsMapIter->second.size());

      std::transform(opsMapIter->second.begin(), opsMapIter->second.end(),
            oldOpsMapIter->second.begin(), std::back_inserter(diffIdOpsMap[opsMapIter->first]),
            diff);
   }

   return diffIdOpsMap;
}

/**
* Returns one op list with the sum over all diff op lists from all ids. If there isn't an old
* list, returns an empty list
*/
ClientOps::OpsList ClientOps::getDiffSumOpsList() const
{
   OpsList diffOpsList;

   std::transform(sumOpsList.begin(), sumOpsList.end(), oldSumOpsList.begin(),
         std::back_inserter(diffOpsList), diff);

   return diffOpsList;
}

/**
* Call when all data for the current data set is collected. Moves current data to old data.
*/
void ClientOps::clear()
{
   const std::lock_guard<Mutex> lock(idOpsMapMutex);
   oldIdOpsMap.clear();
   std::swap(idOpsMap, oldIdOpsMap);
   oldSumOpsList.clear();
   std::swap(sumOpsList, oldSumOpsList);
}

/**
* Request client ops data from a node and returns an unordered map <id, OpsList> .
*/
ClientOpsRequestor::IdOpsUnorderedMap ClientOpsRequestor::request(Node& node, bool perUser)
{
   uint64_t currentID = ~0ULL;
   bool moreData = false;
   uint64_t numOps = 0;

   IdOpsUnorderedMap resultMap;

   do
   {
      GetClientStatsMsg getClientStatsMsg(currentID);

      if (perUser)
         getClientStatsMsg.addMsgHeaderFeatureFlag(GETCLIENTSTATSMSG_FLAG_PERUSERSTATS);

      const auto respMsg = MessagingTk::requestResponse(node, getClientStatsMsg,
            NETMSGTYPE_GetClientStatsResp);

      if (!respMsg)
      {
         LOG(GENERAL, DEBUG, "Node is not responding: " + node.getNodeIDWithTypeStr());
         return ClientOpsRequestor::IdOpsUnorderedMap();
      }

      GetClientStatsRespMsg* respMsgCast = static_cast<GetClientStatsRespMsg*>(respMsg.get());
      std::vector<uint64_t> dataVector;
      respMsgCast->getStatsVector().swap(dataVector);

      moreData = !!dataVector.at(NODE_OPS_POS_MORE_DATA);
      numOps = dataVector.at(NODEOPS_POS_NUMOPS);

      if (dataVector.at(NODE_OPS_POS_LAYOUT_VERSION) != OPCOUNTER_VEC_LAYOUT_VERS)
      {
         LOG(GENERAL, ERR, "Protocol version mismatch in received Message from "
               + node.getNodeIDWithTypeStr());
         return ClientOpsRequestor::IdOpsUnorderedMap();
      }

      auto iter = dataVector.begin();
      iter += NODE_OPS_POS_FIRSTDATAELEMENT;
      unsigned counter = 0;
      ClientOps::OpsList opsList;

      for (; iter != dataVector.end(); iter++)
      {
         if (counter == 0)
            currentID = *iter;
         else
            opsList.push_back(*iter);

         counter++;

         if (counter > numOps)
         {
            resultMap.insert(std::make_pair(currentID, std::move(opsList)));

            opsList.clear();
            counter = 0;
         }
      }

      if (counter != 0)
      {
         LOG(GENERAL, ERR,
               "Reported length of ClientStats OpsList doesn't match the actual length.");
         return ClientOpsRequestor::IdOpsUnorderedMap();
      }
   }
   while (moreData);

   return resultMap;
}
