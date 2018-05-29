#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <app/App.h>

class Program
{
   public:
      static int main(int argc, char** argv);
   
   private:
      Program() {}
      
      static App* app;
      
   public:
      // getters & setters
      static App* getApp()
      {
         return app;
      }
      
};

#endif /*PROGRAM_H_*/
