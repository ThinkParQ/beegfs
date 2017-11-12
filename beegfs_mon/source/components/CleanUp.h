#ifndef CLEANUP_H_
#define CLEANUP_H_

#include <common/threading/PThread.h>
#include <common/nodes/NodeStoreServers.h>

class App;

class CleanUp : public PThread
{
   public:
      CleanUp(App* app);

   private:
      App* const app;
      virtual void run() override;
      void loop();
      void dropIdleConns();
      unsigned dropIdleConnsByStore(NodeStoreServers* nodes);

};


#endif /* CLEANUP_H_ */
