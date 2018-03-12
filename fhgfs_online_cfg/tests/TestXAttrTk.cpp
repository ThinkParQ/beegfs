#include <tests/TestRunnerBase.h>
#include <chrono>
#include <linux/limits.h>

#include <toolkit/XAttrTk.h>
#include "TestXAttrTk.h"


REGISTER_TEST_SUITE(TestXAttrTk);

TestXAttrTk::TestXAttrTk():
path("./test-xattr.txt"),
name1("user.test1"),
name2("user.test2"),
name3("user.test3"),
name4("user.test4"),
name5("user.test5"),
value1({'1', '1', '1', '\0'}),
value2({'2', '2', '2', '\0'}),
value3({'3', '3', '3', '\0'})
{
}

TestXAttrTk::~TestXAttrTk()
{
}

void TestXAttrTk::setUp()
{
   int fd = creat(path.c_str(), 0600);
   close(fd);
}

void TestXAttrTk::tearDown()
{
   unlink(path.c_str());
}

void TestXAttrTk::testGetXAttrs()
{
   XAttrMap attrs;

   //No attributes
   XAttrTk::getXAttrs(path, attrs);

   CPPUNIT_ASSERT_EQUAL((size_t) 0, attrs.size());

   //A few attributes
   lsetxattr(path.c_str(), name1.c_str(), "111\0", 4, XATTR_CREATE);
   lsetxattr(path.c_str(), name2.c_str(), "222\0", 4, XATTR_CREATE);
   lsetxattr(path.c_str(), name3.c_str(), "333\0", 4, XATTR_CREATE);

   XAttrTk::getXAttrs(path, attrs);

   CPPUNIT_ASSERT_EQUAL((size_t) 3, attrs.size());

   //Attribute values
   CPPUNIT_ASSERT(value1 == attrs[name1]);
   CPPUNIT_ASSERT(value2 == attrs[name2]);
   CPPUNIT_ASSERT(value3 == attrs[name3]);

   //Clear attributes
   removexattr(path.c_str(), name1.c_str());
   removexattr(path.c_str(), name2.c_str());
   removexattr(path.c_str(), name3.c_str());
   attrs.clear();

   //Large number of attributes
   const size_t numAttrs = 100;
   setLargeNumberAttrs(numAttrs);

   XAttrTk::getXAttrs(path, attrs);

   CPPUNIT_ASSERT_EQUAL(numAttrs, attrs.size());
}

void TestXAttrTk::testGetXAttrNames()
{
   //No attributes
   std::vector<std::string> names = XAttrTk::getXAttrNames(path);

   CPPUNIT_ASSERT(names.empty());

   //Some attributes
   lsetxattr(path.c_str(), name1.c_str(), "111\0", 4, XATTR_CREATE);
   lsetxattr(path.c_str(), name2.c_str(), "222\0", 4, XATTR_CREATE);
   lsetxattr(path.c_str(), name3.c_str(), "333\0", 4, XATTR_CREATE);

   names = XAttrTk::getXAttrNames(path);

   CPPUNIT_ASSERT_EQUAL((size_t) 3, names.size());
   CPPUNIT_ASSERT(std::find(names.begin(), names.end(), name1) != names.end());
   CPPUNIT_ASSERT(std::find(names.begin(), names.end(), name2) != names.end());
   CPPUNIT_ASSERT(std::find(names.begin(), names.end(), name3) != names.end());

   //Clear attributes
   removexattr(path.c_str(), name1.c_str());
   removexattr(path.c_str(), name2.c_str());
   removexattr(path.c_str(), name3.c_str());
   names.clear();

   //Large number of attributes
   const size_t numAttrs = 100;
   setLargeNumberAttrs(numAttrs);

   names = XAttrTk::getXAttrNames(path);

   CPPUNIT_ASSERT_EQUAL(numAttrs, names.size());
}

void TestXAttrTk::testSetXAttrs()
{
   //Attribute value terminated with \0
   std::vector<char> value1(2, 0);
   value1[0] = '1';

   //Attribute value not terminated with \0
   std::vector<char> value2(2, 0);
   value2[0] = 'Z';
   value2[1] = 'A';

   //Attribute value with non-printable chars
   std::vector<char> value3(2, 0);
   value3[0] = '\1';
   value3[1] = '\4';

   //Attribute value with \0 in the middle
   std::vector<char> value4(3, 0);
   value4[0] = 'X';
   value4[1] = '\0';
   value4[2] = 'A';

   //Empty attribute value
   std::vector<char> value5(1, 0);

   XAttrMap attrs;
   attrs[name1] = value1;
   attrs[name2] = value2;
   attrs[name3] = value3;
   attrs[name4] = value4;
   attrs[name5] = value5;

   XAttrTk::setXAttrs(path, attrs);

   XAttrMap retrievedAttrs;
   XAttrTk::getXAttrs(path, retrievedAttrs);

   CPPUNIT_ASSERT_EQUAL((size_t) 5, retrievedAttrs.size());

   //Attribute values
   std::vector<char> value = retrievedAttrs[name1];
   CPPUNIT_ASSERT_EQUAL('1', value[0]);
   CPPUNIT_ASSERT_EQUAL('\0', value[1]);
   CPPUNIT_ASSERT_EQUAL((size_t) 2, value.size());

   value = retrievedAttrs[name2];
   CPPUNIT_ASSERT_EQUAL('Z', value[0]);
   CPPUNIT_ASSERT_EQUAL('A', value[1]);
   CPPUNIT_ASSERT_EQUAL((size_t) 2, value.size());

   value = retrievedAttrs[name3];
   CPPUNIT_ASSERT_EQUAL('\1', value[0]);
   CPPUNIT_ASSERT_EQUAL('\4', value[1]);
   CPPUNIT_ASSERT_EQUAL((size_t) 2, value.size());

   value = retrievedAttrs[name4];
   CPPUNIT_ASSERT_EQUAL('X', value[0]);
   CPPUNIT_ASSERT_EQUAL('\0', value[1]);
   CPPUNIT_ASSERT_EQUAL('A', value[2]);
   CPPUNIT_ASSERT_EQUAL((size_t) 3, value.size());

   value = retrievedAttrs[name5];
   CPPUNIT_ASSERT_EQUAL('\0', value[0]);
   CPPUNIT_ASSERT_EQUAL((size_t) 1, value.size());
}

void TestXAttrTk::testSetInvalidXAttrs()
{
   //Attribute value with \0 in the middle
   std::vector<char> value(2, 0);
   value[0] = 'X';

   XAttrMap attrs;
   attrs["invalidname"] = value;

   CPPUNIT_ASSERT_THROW(XAttrTk::setXAttrs(path, attrs), XAttrException);

   attrs.clear();
   attrs[""] = value;

   CPPUNIT_ASSERT_THROW(XAttrTk::setXAttrs(path, attrs), XAttrException);

   //Long name
   const size_t length = XATTR_NAME_MAX + 1;
   std::vector<char> name(length, 'x');
   name[length] = 0;
   std::string longName = name.data();

   attrs[longName] = value1;
   CPPUNIT_ASSERT_THROW(XAttrTk::setXAttrs(path, attrs), XAttrException);
}

void TestXAttrTk::testGetXAttrValue()
{
   XAttrValue value(2, 0);
   value[0] = 'A';

   XAttrMap attrs;
   attrs[name1] = value;

   XAttrTk::setXAttrs(path, attrs);

   XAttrValue retrievedValue = XAttrTk::getXAttrValue(path, name1);

   //Attribute values
   CPPUNIT_ASSERT_EQUAL('A', retrievedValue[0]);
   CPPUNIT_ASSERT_EQUAL('\0', retrievedValue[1]);
   CPPUNIT_ASSERT_EQUAL((size_t) 2, retrievedValue.size());
}

void TestXAttrTk::testGetInvalidXAttrs()
{
   XAttrMap attrs;

   CPPUNIT_ASSERT_THROW(XAttrTk::getXAttrValue(path, name1), XAttrException);
   CPPUNIT_ASSERT_THROW(XAttrTk::getXAttrValue(path, ""), XAttrException);
}

void TestXAttrTk::setLargeNumberAttrs(size_t numAttrs)
{
   for (size_t i = 0; i < numAttrs; i++)
   {
      std::string name("user.");
      name += std::to_string(i);

      lsetxattr(path.c_str(), name.c_str(), "X", 1, XATTR_CREATE);

      CPPUNIT_ASSERT_EQUAL(0, errno);
   }
}
