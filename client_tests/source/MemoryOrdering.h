#ifndef MemoryOrdering_h_uSfTNDdPkkCz5vjNP0rgd
#define MemoryOrdering_h_uSfTNDdPkkCz5vjNP0rgd

#include <cppunit/extensions/HelperMacros.h>

class MemoryOrdering : public CPPUNIT_NS::TestFixture {
   CPPUNIT_TEST_SUITE(MemoryOrdering);

   CPPUNIT_TEST(consistency);

   CPPUNIT_TEST_SUITE_END();

   protected:
      void consistency();
};

#endif
