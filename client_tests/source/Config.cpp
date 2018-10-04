#include "Config.h"
#include <getopt.h>
#include <iostream>
#include <string>

std::string Config::dir1;
std::string Config::dir2;
bool Config::useVectorFunctions;

enum { F_DIR_1, F_DIR_2, F_USE_VECTORS, FLAG_COUNT };

static int requiredOptions[] = {
   F_DIR_2, F_DIR_2
};

static struct option options[] = {
   { "dir1",    required_argument, nullptr, F_DIR_1 },
   { "dir2",    required_argument, nullptr, F_DIR_2 },
   { "vectors", no_argument,       nullptr, F_USE_VECTORS },
   { nullptr, 0, nullptr, 0 },
};

static const char* flagNameFor(int flag)
{
   for(auto& opt : options)
   {
      if(opt.val == flag)
         return opt.name;
   }

   return "(unknown flag)";
}

bool Config::parse(int argc, char* argv[])
{
   bool flagsSeen[FLAG_COUNT] = {};
   bool fail = false;

   while(true)
   {
      int flag;

      flag = getopt_long(argc, argv, "", options, nullptr);
      if(flag == -1)
         break;

      if(flagsSeen[flag])
      {
         std::cout << "error: duplicate value " << flagNameFor(flag) << "\n";
         fail = true;
      }

      flagsSeen[flag] = true;

      switch(flag)
      {
         case F_DIR_1:
            dir1 = optarg;
            break;

         case F_DIR_2:
            dir2 = optarg;
            break;

         case F_USE_VECTORS:
            useVectorFunctions = true;
            break;
      }
   }

   for(int required : requiredOptions)
   {
      if(!flagsSeen[required])
      {
         std::cout << "error: required --" << flagNameFor(required) << " missing\n";
         fail = true;
      }
   }

   return !fail;
}
