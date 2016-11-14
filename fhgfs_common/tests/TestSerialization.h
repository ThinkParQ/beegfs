#ifndef TESTSERIALIZATION_H_
#define TESTSERIALIZATION_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <cstring>

#include <common/net/message/NetMessage.h>
#include <common/storage/striping/StripePattern.h>

class TestSerialization : public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestSerialization);

   CPPUNIT_TEST(testByteswap);

   CPPUNIT_TEST(testStr);
   CPPUNIT_TEST(testStrAlign4);
   CPPUNIT_TEST(testChar);
   CPPUNIT_TEST(testBool);
   CPPUNIT_TEST(testShort);
   CPPUNIT_TEST(testUShort);
   CPPUNIT_TEST(testInt);
   CPPUNIT_TEST(testUInt);
   CPPUNIT_TEST(testUInt8);
   CPPUNIT_TEST(testUInt16);
   CPPUNIT_TEST(testInt64);
   CPPUNIT_TEST(testUInt64);
   CPPUNIT_TEST(testTuple);
   CPPUNIT_TEST(testNumNodeID);
   CPPUNIT_TEST(testStorageTargetInfo);
   CPPUNIT_TEST(testQuotaData);
   CPPUNIT_TEST(testPath);
   CPPUNIT_TEST(testBitStore);
   CPPUNIT_TEST(testPathInfo);
   CPPUNIT_TEST(testStatData);

   CPPUNIT_TEST(testUInt8Collections);
   CPPUNIT_TEST(testUInt16Collections);
   CPPUNIT_TEST(testUInt64Collections);
   CPPUNIT_TEST(testInt64Collections);
   CPPUNIT_TEST(testCharVector);
   CPPUNIT_TEST(testUIntCollections);
   CPPUNIT_TEST(testIntCollections);
   CPPUNIT_TEST(testStringCollections);
   CPPUNIT_TEST(testHighRestStatCollections);
   CPPUNIT_TEST(testNicAddressList);
   CPPUNIT_TEST(testNodeList);

   CPPUNIT_TEST(testBuddyMirrorPattern);
   CPPUNIT_TEST(testRaid0Pattern);
   CPPUNIT_TEST(testRaid10Pattern);
   CPPUNIT_TEST(testFsckDirInode);
   CPPUNIT_TEST(testFsckFileInode);
   CPPUNIT_TEST(testEntryInfo);
   CPPUNIT_TEST(testEntryInfoWithDepth);

   CPPUNIT_TEST(testMkLocalDirMsg);
   CPPUNIT_TEST(testSetXAttrMsg);

   CPPUNIT_TEST(testSerializeAs);

   CPPUNIT_TEST_SUITE_END();

   private:
      void testByteswap();

      void testStr();
      void testStrAlign4();
      void testChar();
      void testBool();
      void testShort();
      void testUShort();
      void testInt();
      void testUInt();
      void testUInt8();
      void testUInt16();
      void testInt64();
      void testUInt64();
      void testTuple();
      void testNumNodeID();
      void testStorageTargetInfo();
      void testQuotaData();
      void testPath();
      void testBitStore();
      void testPathInfo();
      void testStatData();

      void testUInt8Collections();
      void testUInt16Collections();
      void testUInt64Collections();
      void testInt64Collections();
      void testCharVector();
      void testUIntCollections();
      void testIntCollections();
      void testStringCollections();
      void testHighRestStatCollections();
      void testNicAddressList();
      void testNodeList();

      void testBuddyMirrorPattern();
      void testRaid0Pattern();
      void testRaid10Pattern();
      void testFsckDirInode();
      void testFsckFileInode();
      void testEntryInfo();
      void testEntryInfoWithDepth();

      void testMkLocalDirMsg();
      void testSetXAttrMsg();

      void testSerializeAs();

      bool memoryEquals(const void* first, const void* second, size_t size)
      {
         const char* a = (const char*) first;
         const char* b = (const char*) second;

         size_t pos = 0;
         while (pos < size)
         {
            if(a[pos] == b[pos])
            {
               pos++;
               continue;
            }

            std::cerr << "\nfound mismatch at position " << pos << ": \n";

            for(int dumped = 0; dumped < 16 && pos + dumped < size; dumped++)
               std::cerr << int(unsigned(a[pos + dumped]) ) << " ";

            std::cerr << "\n";

            for(int dumped = 0; dumped < 16 && pos + dumped < size; dumped++)
               std::cerr << int(unsigned(b[pos + dumped]) ) << " ";

            return false;
         }

         return true;
      }

      template<size_t Size>
      void testStripePattern(StripePattern& pattern, const char (&expected)[Size])
      {
         {
            Serializer ser;
            ser % pattern;
            CPPUNIT_ASSERT(ser.size() == Size);
         }

         char outBuf[Size];

         {
            Serializer ser(outBuf, Size);
            ser % pattern;
            CPPUNIT_ASSERT(ser.good());
            CPPUNIT_ASSERT(ser.size() == Size);
            CPPUNIT_ASSERT(memoryEquals(outBuf, expected, Size) );
         }

         {
            Deserializer des(outBuf, Size);
            StripePattern* read = NULL;

            des % read;

            CPPUNIT_ASSERT(des.good());
            CPPUNIT_ASSERT(read->stripePatternEquals(&pattern) );

            delete read;
         }

         {
            Deserializer des(outBuf, Size - 1);
            StripePattern* read = NULL;

            des % read;

            CPPUNIT_ASSERT(!des.good());
            CPPUNIT_ASSERT(read == NULL);
         }
      }

      struct NoModifier
      {
         template<typename T> const T& operator()(const T& t) { return t; }
         template<typename T>       T& operator()(      T& t) { return t; }
      };

      template<typename Object, typename Modifier, typename Eq>
      void testObject(Object& obj, const char* expBegin, const char* expEnd, Modifier mod, Eq eq)
      {
         const size_t Size = expEnd - expBegin;

         {
            Serializer ser;
            ser % mod((const Object&) obj);
            CPPUNIT_ASSERT(ser.size() == Size);
         }

         // have valgrind catch buffer overflows
         std::vector<char> outBuf(Size);

         {
            Serializer ser(&outBuf[0], Size);

            ser % mod((const Object&) obj);
            CPPUNIT_ASSERT(ser.good());
            CPPUNIT_ASSERT(ser.size() == Size);
            CPPUNIT_ASSERT(memoryEquals(&outBuf[0], expBegin, Size));
         }

         {
            Deserializer des(&outBuf[0], Size);
            Object output;

            des % mod(output);
            CPPUNIT_ASSERT(des.good());
            CPPUNIT_ASSERT(des.size() == Size);
            CPPUNIT_ASSERT(eq(obj, output));
         }

         {
            Deserializer des(&outBuf[0], Size - 1);
            Object output;

            des % mod(output);
            CPPUNIT_ASSERT(!des.good());
         }
      }

      template<typename Object, size_t Size, typename Modifier, typename Eq>
      void testObject(Object& obj, const char (&expected)[Size], Modifier mod, Eq eq)
      {
         testObject(obj, expected, expected + Size, mod, eq);
      }

      template<typename Object, size_t Size>
      void testObject(Object& obj, const char (&expected)[Size])
      {
         testObject(obj, expected, NoModifier(), std::equal_to<Object>());
      }

      template<typename Primitive, size_t Size>
      void testPrimitiveType(Primitive value, const char (&expected)[Size])
      {
         testObject(value, expected);
      }

      template<typename ElemEqT>
      struct CollectionEq
      {
         ElemEqT eq;

         CollectionEq(ElemEqT eq) : eq(eq) {}

         template<typename CollectionT>
         bool operator()(const CollectionT& l, const CollectionT& r) const
         {
            return std::equal(l.begin(), l.end(), r.begin(), eq);
         }
      };

      template<typename Collection, size_t Items, typename Eq>
      void testCollection(const typename Collection::value_type (&data)[Items],
         const std::vector<char>& expected, Eq eq)
      {
         Collection collection(data, data + Items);
         testObject(collection, &expected.front(), &expected.back() + 1, NoModifier(),
            CollectionEq<Eq>(eq));
      }

      template<typename Collection, size_t Items, size_t Size, typename Eq>
      void testCollection(const typename Collection::value_type (&data)[Items],
         const char (&expected)[Size], Eq eq)
      {
         Collection collection(data, data + Items);
         testObject(collection, expected, expected + Size, NoModifier(), CollectionEq<Eq>(eq));
      }

      template<typename Collection, size_t Items, size_t Size>
      void testCollection(const typename Collection::value_type (&data)[Items],
         const char (&expected)[Size])
      {
         testCollection<Collection>(data, expected,
               std::equal_to<typename Collection::value_type>() );
      }

      template<typename Message, size_t Size, typename Eq>
      void testMessage(Message& msg, const char (&expected)[Size], Eq eq)
      {
         CPPUNIT_ASSERT(msg.serializeMessage(NULL, -1).second == Size + NETMSG_HEADER_LENGTH);

         // have valgrind catch buffer overflows
         std::vector<char> outBuf(Size);

         Serializer ser(&outBuf[0], Size);
         msg.serializePayload(ser);
         CPPUNIT_ASSERT(ser.good() );
         CPPUNIT_ASSERT(memoryEquals(&outBuf[0], expected, Size) );

         Message output;

         CPPUNIT_ASSERT(output.deserializePayload(&outBuf[0], Size) );
         CPPUNIT_ASSERT(eq(msg, output) );

         CPPUNIT_ASSERT(!output.deserializePayload(&outBuf[0], Size - 1) );
      }
};

#endif
