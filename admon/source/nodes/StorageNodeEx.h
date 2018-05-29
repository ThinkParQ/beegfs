#ifndef STORAGENODEEX_H_
#define STORAGENODEEX_H_

/*
 * StorageNodeEx is based on the class Node from common and extends this class by providing special
 * data structures and methods to handle admon-relevant statistics
 */


#include <common/nodes/Node.h>
#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/HighResolutionStats.h>
#include <common/net/message/admon/GetNodeInfoRespMsg.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/Common.h>


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
         SafeMutexLock mutexLock(&mutex);
         this->highResStats = stats;
         mutexLock.unlock();
      }

      StorageNodeDataContent getContent()
      {
         SafeMutexLock mutexLock(&mutex);
         StorageNodeDataContent res = this->data;
         mutexLock.unlock();
         return res;
      }

      HighResStatsList getHighResData()
      {
         SafeMutexLock mutexLock(&mutex);
         HighResStatsList res = this->highResStats;
         mutexLock.unlock();
         return res; //this->highResData;
      }

      void setContent(StorageNodeDataContent content)
      {
         SafeMutexLock mutexLock(&mutex);
         this->data = content;
         mutexLock.unlock();
      }

      double getWrittenData(std::string *outUnit)
      {
         SafeMutexLock mutexLock(&mutex);
         *outUnit = this->writtenData.unit;
         double res = this->writtenData.amount;
         mutexLock.unlock();
         return res;
      }

      double getReadData(std::string *outUnit)
      {
         SafeMutexLock mutexLock(&mutex);
         *outUnit = this->readData.unit;
         double res = this->readData.amount;
         mutexLock.unlock();
         return res;
      }

      double getNetRecv(std::string *outUnit)
      {
         SafeMutexLock mutexLock(&mutex);
         *outUnit = this->netRecv.unit;
         double res = this->netRecv.amount;
         mutexLock.unlock();
         return res;
      }

      double getNetSend(std::string *outUnit)
      {
         SafeMutexLock mutexLock(&mutex);
         *outUnit = this->netSend.unit;
         double res = this->netSend.amount;
         mutexLock.unlock();
         return res;
      }

      StorageTargetInfoList getStorageTargets()
      {
         SafeMutexLock mutexLock(&mutex);
         StorageTargetInfoList outList = data.storageTargets;
         mutexLock.unlock();
         return outList;
      }

      void setStorageTargets(StorageTargetInfoList storageTargets)
      {
         SafeMutexLock mutexLock(&mutex);
         data.storageTargets = storageTargets;
         mutexLock.unlock();
      }

      void addOrReplaceStorageTarget(StorageTargetInfo target)
      {
         SafeMutexLock mutexLock(&mutex);
         removeStorageTarget(target.getTargetID());
         data.storageTargets.push_back(target);
         mutexLock.unlock();
      }

      void removeStorageTarget(uint16_t targetID)
      {
         SafeMutexLock mutexLock(&mutex);
         StorageTargetInfoListIter iter = data.storageTargets.begin();
         while (iter != data.storageTargets.end())
         {
            if (targetID == iter->getTargetID())
            {
               data.storageTargets.erase(iter);
               break;
            }
         }
         mutexLock.unlock();
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

#endif /*STORAGENODEEX_H_*/
