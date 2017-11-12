#ifndef NODELISTREQUESTOR_H_
#define NODELISTREQUESTOR_H_

#include <common/threading/PThread.h>

class App;

class NodeListRequestor : public PThread
{
   public:
      NodeListRequestor(App* app);

   private:
      App* const app;
      virtual void run() override;
      void requestLoop();
      bool getMgmtNodeInfo();
};

#endif /*NODELISTREQUESTOR_H_*/
