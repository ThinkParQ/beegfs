#ifndef METANODEEX_H_
#define METANODEEX_H_

/*
 * MetaNodeEx is based on the class Node from common and extends this class by providing special
 * data structures and methods to handle admon-relevant statistics
 */

#include <common/threading/SafeMutexLock.h>
#include <common/nodes/Node.h>
#include <common/net/message/admon/GetNodeInfoRespMsg.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/Common.h>

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
         SafeMutexLock mutexLock(&mutex);
         MetaNodeDataContent res = this->data;
         mutexLock.unlock();
         return res;
      }

      void setContent(MetaNodeDataContent content)
      {
         SafeMutexLock mutexLock(&mutex);
         this->data = content;
         mutexLock.unlock();
      }

      void setHighResStatsList(HighResStatsList stats)
      {
         SafeMutexLock mutexLock(&mutex);
         this->highResStats = stats;
         mutexLock.unlock();
      }

      HighResStatsList getHighResData()
      {
         SafeMutexLock mutexLock(&mutex);
         HighResStatsList res = this->highResStats;
         mutexLock.unlock();
         return res;
      }

      void setGeneralInformation(GeneralNodeInfo& info)
      {
         SafeMutexLock mutexLock(&mutex);
         this->generalInfo = info;
         mutexLock.unlock();
      }

      GeneralNodeInfo getGeneralInformation()
      {
         SafeMutexLock mutexLock(&mutex);
         GeneralNodeInfo info = this->generalInfo;
         mutexLock.unlock();
         return info;
      }


};

#endif /*METANODEEX_H_*/
