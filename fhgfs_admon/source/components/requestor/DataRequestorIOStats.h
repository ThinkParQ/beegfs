#ifndef DATAREQUESTORIOSTATS_H_
#define DATAREQUESTORIOSTATS_H_

/*
 * The data requestor is a PThread, which periodically polls the nodes for data to display. It
 * creates work packages (which send messages to the nodes and take care of the response) in fixed
 * intervals
 */

#include <common/app/log/LogContext.h>
#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/nodes/Node.h>
#include <common/threading/PThread.h>
#include <nodes/NodeStoreMetaEx.h>
#include <nodes/NodeStoreStorageEx.h>
#include <components/worker/RequestMetaDataWork.h>
#include <components/worker/RequestStorageDataWork.h>

class DataRequestorIOStats: public PThread
{
   private:
      NodeStoreMetaEx *metaNodes;
      NodeStoreStorageEx *storageNodes;
      MultiWorkQueue *workQueue;
      LogContext log;
      int queryInterval;
      Time t;
      void requestLoop();

   public:
      /*
       * @param metaNodes Pointer to the global meta node store of the app
       * @param storageNodes Pointer to the global storage node store of the app
       * @param workQueue Pointer to the global work queue of the app
       * @param queryInterval The interval, in which to create work packets (in secondes)
       */
      DataRequestorIOStats(NodeStoreMetaEx *metaNodes, NodeStoreStorageEx *storageNodes,
         MultiWorkQueue *workQueue, int queryInterval)
          : PThread("DataReq-IOStats"),
            metaNodes(metaNodes),
            storageNodes(storageNodes),
            workQueue(workQueue),
            log("DataReq-IOStats"),
            queryInterval(queryInterval * 1000) //it comes in seconds, not in milliseconds
      {
      }

      virtual ~DataRequestorIOStats()
      {
      }

      virtual void run();
};

#endif /*DATAREQUESTORIOSTATS_H_*/
