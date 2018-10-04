#include "SignalHandler.h"

#include <common/app/log/Logger.h>
#include <app/App.h>

#include <csignal>

App* SignalHandler::app = nullptr;

void SignalHandler::registerSignalHandler(App* app)
{
   SignalHandler::app = app;
   signal(SIGINT, SignalHandler::handle);
   signal(SIGTERM, SignalHandler::handle);
}


void SignalHandler::handle(int sig)
{
   // reset signal handling to default
   signal(sig, SIG_DFL);

   if (Logger::isInitialized())
   {
      switch(sig)
      {
         case SIGINT:
         {
            LOG(GENERAL, CRITICAL, "Received a SIGINT. Shutting down...");
         } break;

         case SIGTERM:
         {
            LOG(GENERAL, CRITICAL, "Received a SIGTERM. Shutting down...");
         } break;

         default:
         {
            // shouldn't happen
            LOG(GENERAL, CRITICAL, "Received an unknown signal. Shutting down...");
         } break;
      }
   }

   if (app != nullptr)
   {
      app->stopComponents();
   }
}