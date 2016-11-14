#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <app/App.h>


/**
 * Represents the static program. It creates an App object to represent the running instance of
 * the program and provides a getter for the App object.
 */
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
