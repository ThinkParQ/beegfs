#include <common/net/message/nodes/GetNodeCapacityPoolsMsg.h>
#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/Time.h>
#include <common/nodes/NodeStore.h>
#include <common/nodes/TargetCapacityPools.h>
#include <program/Program.h>
#include <app/App.h>
#include <app/config/Config.h>
#include "InternodeSyncer.h"


InternodeSyncer::InternodeSyncer() throw(ComponentInitException) : PThread("XNodeSync")
{
   log.setContext("XNodeSync");

   this->forcePoolsUpdate = false;
}

InternodeSyncer::~InternodeSyncer()
{
   // nothing to be done here
}


void InternodeSyncer::run()
{
   try
   {
      registerSignalHandler();

      syncLoop();

      log.log(4, "Component stopped.");
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

void InternodeSyncer::syncLoop()
{
   App* app = Program::getApp();

   const int sleepIntervalMS = 3*1000; // 3sec

   const unsigned updateMetaCapacityMS = 10*60*1000; // 10min
   const unsigned updateStorageCapacityMS = 5*60*1000; // 5min
   const unsigned metaCacheSweepNormalMS = 5*1000; // 5sec
   const unsigned metaCacheSweepStressedMS = 2*1000; // 2sec

   Time lastMetaCapacityUpdateT;
   Time lastStorageCapacityUpdateT;
   Time lastMetaCacheSweepT;

   unsigned currentCacheSweepMS = metaCacheSweepNormalMS; // (adapted inside the loop below)


   while(!waitForSelfTerminateOrder(sleepIntervalMS) )
   {
      bool poolsUpdateForced = getAndResetForcePoolsUpdate();

      if( poolsUpdateForced ||
         (lastMetaCapacityUpdateT.elapsedMS() > updateMetaCapacityMS) )
      {
         updateMetaCapacityPools();
         lastMetaCapacityUpdateT.setToNow();
      }

      if( poolsUpdateForced ||
          (lastStorageCapacityUpdateT.elapsedMS() > updateStorageCapacityMS) )
      {
         updateStorageCapacityPools();
         lastStorageCapacityUpdateT.setToNow();
      }

      if(lastMetaCacheSweepT.elapsedMS() > currentCacheSweepMS)
      {
         bool flushTriggered = app->getMetaStore()->cacheSweepAsync();

         currentCacheSweepMS = (flushTriggered ? metaCacheSweepStressedMS : metaCacheSweepNormalMS);

         lastMetaCacheSweepT.setToNow();
      }
   }
}

void InternodeSyncer::updateMetaCapacityPools()
{
   TargetCapacityPools* pools = Program::getApp()->getMetaCapacityPools();

   updateCapacityPools(NODETYPE_Meta, pools);
}

void InternodeSyncer::updateStorageCapacityPools()
{
   TargetCapacityPools* pools = Program::getApp()->getStorageCapacityPools();

   updateCapacityPools(NODETYPE_Storage, pools);
}

void InternodeSyncer::updateCapacityPools(NodeType nodeType, TargetCapacityPools* pools)
{
   NodeStore* mgmtNodes = Program::getApp()->getMgmtNodes();

   Node* node = mgmtNodes->referenceFirstNode();
   if(!node)
      return;

   GetNodeCapacityPoolsMsg msg(nodeType);

   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   GetNodeCapacityPoolsRespMsg* respMsgCast;

   UInt16List listNormal;
   UInt16List listLow;
   UInt16List listEmergency;

   // connect & communicate
   commRes = MessagingTk::requestResponse(
      node, &msg, NETMSGTYPE_GetNodeCapacityPoolsResp, &respBuf, &respMsg);
   if(!commRes)
      goto clean_up;

   // handle result
   respMsgCast = (GetNodeCapacityPoolsRespMsg*)respMsg;

   respMsgCast->parseLists(&listNormal, &listLow, &listEmergency);

   pools->syncPoolsFromLists(listNormal, listLow, listEmergency);

   // cleanup
clean_up:
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   mgmtNodes->releaseNode(&node);
}
