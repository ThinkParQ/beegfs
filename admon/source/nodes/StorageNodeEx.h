#ifndef STORAGENODEEX_H_
#define STORAGENODEEX_H_

/*
 * StorageNodeEx is based on the class Node from common and extends this class by providing special
 * data structures and methods to handle admon-relevant statistics
 */


#include <common/nodes/Node.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/net/message/admon/GetNodeInfoRespMsg.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/Common.h>

#include <mutex>

// forward declaration
class Database;

struct StorageNodeDataContent
{
      uint64_t time; // seconds since the epoch
      bool isResponding;
      unsigned indirectWorkListSize;
      unsigned directWorkListSize;

      int64_t diskSpaceTotal;
      int64_t diskSpaceFree;
      // ammount of written data since last measurement
      int64_t diskRead;
      int64_t diskWrite;
      // average read / write per sec follows
      int64_t diskReadPerSec; // store in MiB, does not need to be float, integer is accurate enough
      int64_t diskWritePerSec; // store in MiB
      unsigned sessionCount;
      StorageTargetInfoList storageTargets;
};

struct StorageNodeDataStore
{
      uint64_t nextUpdate;
      std::list<StorageNodeDataContent> data;
};

struct DataCounter
{
      double amount;
      std::string unit;
      uint64_t divideBy;
};

class StorageNodeEx : public Node
{
   public:
      StorageNodeEx(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP,
         unsigned short portTCP, NicAddressList& nicList);

      void initializeDBData();
      void initialize();

      void update(StorageNodeEx *newNode);

      void setNotResponding();
      void upAgain();


   private:
      StorageNodeDataContent data;
      StorageNodeDataStore oldData; /* the finest store, stores the direct data every five seconds
                                      (if not configured otherwise) */
      StorageNodeDataStore hourlyData;
      StorageNodeDataStore dailyData;
      DataCounter writtenData;
      DataCounter readData;
      DataCounter netRecv;
      DataCounter netSend;
      unsigned maxSizeOldData;
      Database *db;
      GeneralNodeInfo generalInfo;

      HighResStatsList highResStats;

      void average(std::list<StorageNodeDataContent> *originalData, std::list<
         StorageNodeDataContent> *outList);

      void addHighResStatsList(HighResStatsList stats);

   public:

      void setHighResStatsList(HighResStatsList stats)
      {
         const std::lock_guard<Mutex> lock(mutex);
         this->highResStats = stats;
      }

      StorageNodeDataContent getContent()
      {
         const std::lock_guard<Mutex> lock(mutex);
         return this->data;
      }

      HighResStatsList getHighResData()
      {
         const std::lock_guard<Mutex> lock(mutex);
         return this->highResStats;
      }

      void setContent(StorageNodeDataContent content)
      {
         const std::lock_guard<Mutex> lock(mutex);
         this->data = content;
      }

      double getWrittenData(std::string *outUnit)
      {
         const std::lock_guard<Mutex> lock(mutex);
         *outUnit = this->writtenData.unit;
         return this->writtenData.amount;
      }

      double getReadData(std::string *outUnit)
      {
         const std::lock_guard<Mutex> lock(mutex);
         *outUnit = this->readData.unit;
         return this->readData.amount;
      }

      double getNetRecv(std::string *outUnit)
      {
         const std::lock_guard<Mutex> lock(mutex);
         *outUnit = this->netRecv.unit;
         return this->netRecv.amount;
      }

      double getNetSend(std::string *outUnit)
      {
         const std::lock_guard<Mutex> lock(mutex);
         *outUnit = this->netSend.unit;
         return this->netSend.amount;
      }

      StorageTargetInfoList getStorageTargets()
      {
         const std::lock_guard<Mutex> lock(mutex);
         return data.storageTargets;
      }

      void setStorageTargets(StorageTargetInfoList storageTargets)
      {
         const std::lock_guard<Mutex> lock(mutex);
         data.storageTargets = storageTargets;
      }

      void addOrReplaceStorageTarget(StorageTargetInfo target)
      {
         const std::lock_guard<Mutex> lock(mutex);
         removeStorageTarget(target.getTargetID());
         data.storageTargets.push_back(target);
      }

      void removeStorageTarget(uint16_t targetID)
      {
         const std::lock_guard<Mutex> lock(mutex);
         StorageTargetInfoListIter iter = data.storageTargets.begin();
         while (iter != data.storageTargets.end())
         {
            if (targetID == iter->getTargetID())
            {
               data.storageTargets.erase(iter);
               break;
            }
         }
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

#endif /*STORAGENODEEX_H_*/
