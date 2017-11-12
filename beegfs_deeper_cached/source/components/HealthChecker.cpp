#include "HealthChecker.h"
#include <program/Program.h>



void HealthChecker::run()
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   Logger* log = Logger::getLogger();
   CacheWorkManager* cacheMgr = app->getWorkQueue();

   try
   {
      while(!waitForSelfTerminateOrder(cfg->getLogHealthCheckIntervalSec() * 1000) )
      {
         std::string stats;
         bool someStats = cacheMgr->healthCheck(stats);
         if(someStats)
         {
            log->log(Log_DEBUG, "stats", stats);
         }
      }
   }
   catch(std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }
}

