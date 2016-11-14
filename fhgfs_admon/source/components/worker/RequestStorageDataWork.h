#ifndef REQUESTSTORAGEDATAWORK_H_
#define REQUESTSTORAGEDATAWORK_H_

/*
 * RequestStorageDataWork sends a message to a specific storage node to request
 * the statistics of that node
 */

#include <common/components/worker/Work.h>
#include <common/net/message/admon/RequestStorageDataMsg.h>
#include <common/net/message/admon/RequestStorageDataRespMsg.h>
#include <common/toolkit/StringTk.h>
#include <nodes/StorageNodeEx.h>
#include <nodes/NodeStoreStorageEx.h>

class RequestStorageDataWork : public Work
{
   public:
      /*
       * @param node storage node, which is contacted
       */
      RequestStorageDataWork(std::shared_ptr<StorageNodeEx> node) :
         node(std::move(node)), log("Request Storage Data Work")
      { }

      virtual ~RequestStorageDataWork() {}

      void process(char* bufIn, unsigned bufInLen, char* bufOut,
         unsigned bufOutLen);

   private:
      std::shared_ptr<StorageNodeEx> node;
      LogContext log;
};

#endif /*REQUESTSTORAGEDATAWORK_H_*/
