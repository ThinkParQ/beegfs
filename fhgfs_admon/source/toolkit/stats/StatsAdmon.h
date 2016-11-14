#ifndef STATSADMON_H_
#define STATSADMON_H_


#include <common/toolkit/TimeAbs.h>
#include <common/clientstats/ClientStats.h>

#include <tinyxml.h>


class StatsAdmon : public ClientStats
{
   public:
      StatsAdmon(CfgClientStatsOptions *cfgOptions) : ClientStats(cfgOptions) {};

      StatsAdmon(ClientStats *stats) : ClientStats(stats) {}

      void addStatsToXml(TiXmlElement *rootElement, bool doUserStats);

   private:
      void addCountersToElement(UInt64Vector *statsVec, TiXmlElement *xmlParent);
};

#endif /* STATSADMON_H_ */
