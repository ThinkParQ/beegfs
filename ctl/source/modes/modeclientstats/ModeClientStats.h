#ifndef MODECLIENTSTATS_H_
#define MODECLIENTSTATS_H_

#include <common/Common.h>
#include <common/nodes/ClientOps.h>
#include <common/nodes/NodeStoreServers.h>
#include <modes/Mode.h>

class App;

class ModeClientStats : public Mode
{
   public:
      ModeClientStats() :
         nodeType(NODETYPE_Meta),
         perUser(false),
         allStats(false),
         interval(5),
         perInterval(false),
         maxLines(0),
         filter(""),
         rwUnitBytes(false),
         names(false),
         nodeIDOnly(0),
         diffLoop(false)
      {}

      virtual int execute() override;
      static void printHelp();

      void setPerUser(bool perUser)
      {
         this->perUser = perUser;
      }

   private:
      App* app;

      std::vector<NodeHandle> nodeVector;

      NodeType nodeType;
      bool perUser;
      bool allStats;
      unsigned interval;
      bool perInterval;
      unsigned maxLines;
      std::string filter;
      bool rwUnitBytes;
      bool names;
      uint16_t nodeIDOnly;

      bool diffLoop;

      ClientOps clientOps;

      void addNodesToVector();
      void loop();

      enum class ParseResult
      {
         OK,
         EMPTY,
         DIRTY
      };

      ParseResult parseAndPrintOpsList(
            const std::string& id, const ClientOps::OpsList& opsList) const;

      void makeStdinNonblocking() const;
      void makeStdinBlocking() const;
      bool waitForInput(int timeoutSecs) const;
      std::vector<std::pair<int64_t, ClientOps::OpsList>> sortedVectorByOpsSum(const ClientOps::IdOpsMap &idOpsMap) const;
};

#endif /* MODECLIENTSTATS_H_ */
