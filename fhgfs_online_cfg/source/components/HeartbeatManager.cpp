#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/SocketTk.h>
#include <program/Program.h>
#include "HeartbeatManager.h"

HeartbeatManager::HeartbeatManager(DatagramListener* dgramLis) throw(ComponentInitException)
 : PThread("HBeatMgr"),
   log("HBeatMgr"),
   cfg(Program::getApp()->getConfig() ),
   dgramLis(dgramLis),
   metaNodes(Program::getApp()->getMetaNodes() ),
   storageNodes(Program::getApp()->getStorageNodes() )
{
}

HeartbeatManager::~HeartbeatManager()
{
}


void HeartbeatManager::run()
{
   try
   {
      registerSignalHandler();

      requestLoop();

      log.log(4, "Component stopped.");
   }
   catch(std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }

}



void HeartbeatManager::requestLoop()
{
   int sleepTimeMS = 5000;


   while(!waitForSelfTerminateOrder(sleepTimeMS) )
   {

   } // end of while loop
}


