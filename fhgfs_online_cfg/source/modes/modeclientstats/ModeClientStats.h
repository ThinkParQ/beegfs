#ifndef MODECLIENTSTATS_H_
#define MODECLIENTSTATS_H_

#include <app/App.h>
#include <common/Common.h>
#include <common/clientstats/CfgClientStatsOptions.h>
#include <common/clientstats/ClientStats.h>
#include <common/nodes/NodeStoreServers.h>
#include <modes/Mode.h>


class ModeClientStats : public Mode
{
   public:

      virtual int execute();

      static void printHelp();

   private:
      NodeStoreServers* nodeStore;

      CfgClientStatsOptions cfgOptions;

      int addNodesToNodeStore(App* app, NodeStoreServers* mgmtNodes, NodeHandle& mgmtNode);
      void deleteListNodesExceptProvided(std::vector<NodeHandle>& nodes, uint16_t exceptID);
      int  getParams(App* app);
      bool receivePrintLoop();

      void makeStdinNonblocking();
      void makeStdinBlocking();
      bool waitForInput(int timeoutSecs);


   public:
      // setters & getters
      void setPerUser(bool perUser)
      {
         cfgOptions.perUser = true;
      }
};

#endif /* MODECLIENTSTATS_H_ */
