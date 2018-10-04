#ifndef source_Config_h_jEVyxglaXM5kTfYzNu0Hu
#define source_Config_h_jEVyxglaXM5kTfYzNu0Hu

#include <string>

class Config {
   private:
      Config() = delete;

      static std::string dir1;
      static std::string dir2;
      static bool useVectorFunctions;

   public:
      static bool parse(int argc, char* argv[]);

      static const std::string& getDir1() { return dir1; }
      static const std::string& getDir2() { return dir2; }
      static bool getUseVectorFunctions() { return useVectorFunctions; }
};

#endif
