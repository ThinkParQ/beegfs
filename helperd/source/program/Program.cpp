#include <common/toolkit/BuildTypeTk.h>
#include "Program.h"

App* Program::app = NULL;

int Program::main(int argc, char** argv)
{
   BuildTypeTk::checkDebugBuildTypes();

   AbstractApp::runTimeInitsAndChecks(); // must be called before creating a new App

   app = new App(argc, argv);

   app->startInCurrentThread();

   int appRes = app->getAppResult();

   delete app;

   return appRes;
}
