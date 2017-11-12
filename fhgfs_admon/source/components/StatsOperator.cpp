#include "StatsOperator.h"

#include <common/threading/SafeRWLock.h>


uint32_t StatsOperator::addOrUpdate(uint32_t requestorID, unsigned intervalSecs,
   unsigned maxLines, NodeType nodeType, bool perUser)
{
   SafeRWLock lock(&requestorMapRWLock, SafeRWLock_WRITE);        // W R I T E L O C K

   uint32_t newRequestorID = this->getNextRequestorIDUnlocked();

   DataRequestorStats* requestor = new DataRequestorStats(newRequestorID, intervalSecs,
      maxLines, nodeType, perUser);

   if (requestorID == 0)
   { // a new client stats
      this->requestorMap.insert(DataRequestorStatsMapVal(newRequestorID, requestor));
      requestor->start();

      lock.unlock();                                              // U N L O C K

      return newRequestorID;
   }

   DataRequestorStatsMapIter iter = this->requestorMap.find(requestorID);
   if (iter != this->requestorMap.end() )
   { // a old client stats GUI changed some configurations
      iter->second->stop();
      this->requestorMap.insert(DataRequestorStatsMapVal(newRequestorID, requestor));
      requestor->start();

      lock.unlock();                                              // U N L O C K

      return newRequestorID;
   }
   else
   { // a old client stats GUI but not any more in the list
      this->requestorMap.insert(DataRequestorStatsMapVal(newRequestorID, requestor));
      requestor->start();

      lock.unlock();                                              // U N L O C K

      return newRequestorID;
   }
}

StatsAdmon* StatsOperator::getStats(uint32_t requestorID,
   uint64_t &outDataSequenceID)
{
   StatsAdmon* retVal = NULL;

   SafeRWLock lock(&requestorMapRWLock, SafeRWLock_READ);         // R E A D L O C K

   DataRequestorStatsMapIter iter = this->requestorMap.find(requestorID);

   if (iter != this->requestorMap.end() )
      retVal = iter->second->getStatsCopy(outDataSequenceID);

   lock.unlock();                                                 // U N L O C K

   return retVal;
}

uint32_t StatsOperator::getNextRequestorIDUnlocked()
{
   uint32_t nextID = ++this->lastID;
   uint32_t firstNextID = nextID;

   while ( (nextID == 0) || (this->requestorMap.find(nextID) != this->requestorMap.end() ) )
   {
      nextID = ++this->lastID;

      if (nextID == firstNextID)
      {
         log.log(Log_ERR, "Too many ClientStats connections.");
         nextID = 0;
         break;
      }
   }

   return nextID;
}

uint32_t StatsOperator::getNextRequestorID()
{
   uint32_t retVal;

   SafeRWLock lock(&requestorMapRWLock, SafeRWLock_WRITE);        // W R I T E L O C K

   retVal = getNextRequestorIDUnlocked();

   lock.unlock();                                                 // U N L O C K

   return retVal;
}

void StatsOperator::cleanupStoppedRequestors()
{
   SafeRWLock lock(&requestorMapRWLock, SafeRWLock_WRITE);        // W R I T E L O C K

   for (DataRequestorStatsMapIter iter = this->requestorMap.begin();
      iter != this->requestorMap.end(); )
   {
      if (iter->second->getStatus() == DataRequestorStatsStatus_STOPPED)
      {
         DataRequestorStatsMapIter tmpIter = iter;
         iter++;
         this->requestorMap.erase(tmpIter);
      }
      else
         iter++;
   }

   lock.unlock();                                                 // U N L O C K
}

bool StatsOperator::needUpdate(unsigned requestorID, unsigned intervalSecs, unsigned maxLines,
   NodeType nodeType, bool perUser)
{
   bool retVal = true;

   if (requestorID == 0)
      return true;

   SafeRWLock lock(&requestorMapRWLock, SafeRWLock_READ);         // R E A D L O C K

   DataRequestorStatsMapIter iter = this->requestorMap.find(requestorID);
   if( iter != this->requestorMap.end())
   {
      retVal = !iter->second->getCfgStatsOptions().isEqualForAdmon(intervalSecs, maxLines,
         nodeType, perUser);
   }

   lock.unlock();                                                 // U N L O C K

   return retVal;
}
