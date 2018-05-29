#ifndef SIGNAL_HANDLER_H_
#define SIGNAL_HANDLER_H_

class App;

class SignalHandler
{
   public:
      static void registerSignalHandler(App* app);
      static void handle(int sig);

   private:
      static App* app;
};

#endif