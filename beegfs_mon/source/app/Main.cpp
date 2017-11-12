#include <common/toolkit/BuildTypeTk.h>
#include <app/SignalHandler.h>
#include <app/App.h>

int main(int argc, char** argv)
{
   BuildTypeTk::checkDebugBuildTypes();
   AbstractApp::runTimeInitsAndChecks();

   App app(argc, argv);
   app.startInCurrentThread();

   return app.getAppResult();
}