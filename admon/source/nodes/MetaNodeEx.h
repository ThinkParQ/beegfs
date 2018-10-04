#ifndef METANODEEX_H_
#define METANODEEX_H_

/*
 * MetaNodeEx is based on the class Node from common and extends this class by providing special
 * data structures and methods to handle admon-relevant statistics
 */

#include <common/nodes/Node.h>
#include <common/net/message/admon/GetNodeInfoRespMsg.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/Common.h>

#include <mutex>

// forward declaration
class Database;

struct MetaNodeDataContent
{
   uint64_t time; // seconds since the epoch
   bool isResponding;
   bool isRoot;
   unsigned indirectWorkListSize;
   unsigned directWorkListSize;
   unsigned sessionCount;
   unsigned queuedRequests;
   unsigned workRequests;
};

struct MetaNodeDataStore
{
   uint64_t nextUpdate;
   std::list<MetaNodeDataContent> data;
};

class MetaNodeEx: public Node
{
   public:
      MetaNodeEx(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP,
         unsigned short portTCP, NicAddressList& nicList);

      void initializeDBData();
      void initialize();

      void update(MetaNodeEx* newNode);

      void setNotResponding();
      void upAgain();


   private:
      MetaNodeDataContent data;
      MetaNodeDataStore oldData; /* the finest store, stores the direct data every five seconds
                                    (if not configured otherwise) */
      MetaNodeDataStore hourlyData;
      MetaNodeDataStore dailyData;
      unsigned maxSizeOldData;
      Database *db;
      GeneralNodeInfo generalInfo;
      HighResStatsList highResStats;

      void average(std::list<MetaNodeDataContent> *originalData,
         std::list<MetaNodeDataContent> *outList);
      void addHighResStatsList(HighResStatsList stats);


   public:

      // getters & setters

      MetaNodeDataContent getContent()
      {
         const std::lock_guard<Mutex> lock(mutex);
         return this->data;
      }

      void setContent(MetaNodeDataContent content)
      {
         const std::lock_guard<Mutex> lock(mutex);
         this->data = content;
      }

      void setHighResStatsList(HighResStatsList stats)
      {
         const std::lock_guard<Mutex> lock(mutex);
         this->highResStats = stats;
      }

      HighResStatsList getHighResData()
      {
         const std::lock_guard<Mutex> lock(mutex);
         return this->highResStats;
      }

      void setGeneralInformation(GeneralNodeInfo& info)
      {
         const std::lock_guard<Mutex> lock(mutex);
         this->generalInfo = info;
      }

      GeneralNodeInfo getGeneralInformation()
      {
         const std::lock_guard<Mutex> lock(mutex);
         return this->generalInfo;
      }


};

#endif /*METANODEEX_H_*/
