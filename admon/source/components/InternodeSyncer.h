#ifndef INTERNODESYNCER_H_
#define INTERNODESYNCER_H_

#include <common/app/log/LogContext.h>
#include <common/components/ComponentInitException.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/threading/PThread.h>
#include <common/Common.h>


class InternodeSyncer : public PThread
{
   public:
      InternodeSyncer();
      virtual ~InternodeSyncer();


   private:
      LogContext log;

      virtual void run();
      void syncLoop();

      void dropIdleConns();
      unsigned dropIdleConnsByStore(NodeStoreServers* nodes);

};


#endif /* INTERNODESYNCER_H_ */
