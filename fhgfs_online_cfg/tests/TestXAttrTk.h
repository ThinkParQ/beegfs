#ifndef TESTXATTRSTK_H_
#define TESTXATTRSTK_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

/**
 * Class XAttrTk unit tests.
 */
class TestXAttrTk: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestXAttrTk );
      CPPUNIT_TEST(testGetXAttrs);
      CPPUNIT_TEST(testGetXAttrNames);
      CPPUNIT_TEST(testSetXAttrs);
      CPPUNIT_TEST(testGetXAttrValue);
      CPPUNIT_TEST(testSetInvalidXAttrs);
      CPPUNIT_TEST(testGetInvalidXAttrs);
      CPPUNIT_TEST_SUITE_END();

   public:
      TestXAttrTk();
      virtual ~TestXAttrTk();

      void setUp();
      void tearDown();

      void testGetXAttrs();
      void testGetXAttrNames();
      void testSetXAttrs();
      void testGetXAttrValue();
      void testSetInvalidXAttrs();
      void testGetInvalidXAttrs();

      void setLargeNumberAttrs(size_t numAttrs);

   private:
      const std::string path;
      const std::string name1;
      const std::string name2;
      const std::string name3;
      const std::string name4;
      const std::string name5;
      const std::vector<char> value1;
      const std::vector<char> value2;
      const std::vector<char> value3;
};

#endif /* TESTXATTRSTK_H_ */
