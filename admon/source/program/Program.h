#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <app/App.h>

class Program
{
   public:
      static int main(int argc, char** argv);


   private:
	  static bool restart;

      Program() { }

      static App* app;

      static int startApp(int argc, char** argv);

   public:
      // getters & setters
      static App* getApp()
      {
         return app;
      }

      static void setRestart(bool value)
      {
    	  Program::restart=value;
      }

};

#endif /*PROGRAM_H_*/
