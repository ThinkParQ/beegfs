#ifndef FLATTEST_H_
#define FLATTEST_H_

#include <string>

#include <inttypes.h>

class FlatTest
{
   public:
      FlatTest();

      void setUp();
      void tearDown();

      struct Data {
         uint64_t id;
         char dummy[1024 - sizeof(uint64_t)];

         typedef uint64_t KeyType;
         uint64_t pkey() const { return id; }
      };

   protected:
      std::string dirName;
      std::string fileName;
};

#endif
