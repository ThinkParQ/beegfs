#pragma once

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

