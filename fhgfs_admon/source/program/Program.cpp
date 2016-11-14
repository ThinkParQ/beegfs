#include <common/toolkit/BuildTypeTk.h>
#include "Program.h"


App* Program::app = NULL;
bool Program::restart = false;


int Program::main(int argc, char** argv)
{
   BuildTypeTk::checkDebugBuildTypes();

   int appRes = Program::startApp(argc,argv);

   while (Program::restart)
   {
      Program::restart=false;
      appRes = Program::startApp(argc,argv);
   }

   return appRes;
}

int Program::startApp(int argc, char** argv)
{
   AbstractApp::runTimeInitsAndChecks(); // must be called before creating a new App

   app = new App(argc, argv);

   app->startInCurrentThread();

   app->join();

   int appRes = app->getAppResult();

   delete app;

   return appRes;
}

