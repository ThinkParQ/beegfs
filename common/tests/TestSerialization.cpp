#include <common/storage/EntryInfo.h>
#include <common/toolkit/serialization/Serialization.h>

#include <common/fsck/FsckDirInode.h>
#include <common/fsck/FsckFileInode.h>
#include <common/nodes/Node.h>
#include <common/net/message/storage/attribs/SetXAttrMsg.h>
#include <common/net/message/storage/creating/MkLocalDirMsg.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/EntryInfoWithDepth.h>
#include <common/storage/Path.h>
#include <common/storage/PathInfo.h>
#include <common/storage/StorageTargetInfo.h>
#include <common/storage/quota/QuotaData.h>
#include <common/storage/striping/BuddyMirrorPattern.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/Raid10Pattern.h>

#include <arpa/inet.h>

#include <gtest/gtest.h>

TEST(Serialization, byteswap)
{
   ASSERT_EQ(byteswap16(0x0102), 0x0201);
   ASSERT_EQ(byteswap32(0x01020304), 0x04030201u);
   ASSERT_EQ(byteswap64(0x0102030405060708ULL), 0x0807060504030201ULL);
}



static bool memoryEquals(const void* first, const void* second, size_t size)
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

struct NoModifier
{
   template<typename T> const T& operator()(const T& t) { return t; }
   template<typename T>       T& operator()(      T& t) { return t; }
};

template<typename Object, typename Modifier = NoModifier, typename Eq = std::equal_to<Object>>
void testObject(Object& obj, const char* expBegin, const char* expEnd, Modifier mod = Modifier(),
      Eq eq = Eq())
{
   SCOPED_TRACE("testObject");

   const size_t Size = expEnd - expBegin;

   {
      Serializer ser;
      ser % mod((const Object&) obj);
      ASSERT_EQ(ser.size(), Size);
   }

   // have valgrind catch buffer overflows
   std::vector<char> outBuf(Size);

   {
      Serializer ser(&outBuf[0], Size);

      ser % mod((const Object&) obj);
      ASSERT_TRUE(ser.good());
      ASSERT_EQ(ser.size(), Size);
      ASSERT_TRUE(memoryEquals(&outBuf[0], expBegin, Size));
   }

   {
      Deserializer des(&outBuf[0], Size);
      Object output;

      des % mod(output);
      ASSERT_TRUE(des.good());
      ASSERT_EQ(des.size(), Size);
      ASSERT_TRUE(eq(obj, output));
   }

   {
      Deserializer des(&outBuf[0], Size - 1);
      Object output;

      des % mod(output);
      ASSERT_FALSE(des.good());
   }
}

template<typename Object, size_t Size, typename Modifier = NoModifier,
   typename Eq = std::equal_to<Object>>
void testObject(Object& obj, const char (&expected)[Size], Modifier mod = Modifier(), Eq eq = Eq())
{
   testObject(obj, expected, expected + Size, mod, eq);
}

template<typename T, size_t Size>
void testSimpleSerializer(T value, const char (&expected)[Size])
{
   testObject(value, expected, NoModifier(), std::equal_to<T>());
}



TEST(Serialization, isDirection)
{
   {
      Serializer ser;
      ASSERT_TRUE(ser.isWriting());
      ASSERT_FALSE(ser.isReading());
   }

   {
      Deserializer des(0, 0);
      ASSERT_FALSE(des.isWriting());
      ASSERT_TRUE(des.isReading());
   }
}

TEST(Serialization, primitiveTypes)
{
   {
      const char expectedFalse[] = { 0 };
      const char expectedTrue[] = { 1 };
      testSimpleSerializer<bool>(false, expectedFalse);
      testSimpleSerializer<bool>(true, expectedTrue);
   }

   {
      const char expected[] = { 'X' };
      testSimpleSerializer<char>('X', expected);
   }

   {
      const char expected[] = { '\xff' };
      testSimpleSerializer<uint8_t>(255, expected);
   }

   {
      const char expected[] = { 2, 1 };
      testSimpleSerializer<int16_t>(258, expected);
   }

   {
      const char expected[] = { '\xff', '\xff' };
      testSimpleSerializer<uint16_t>(-1, expected);
   }

   {
      const char expected[] = { 0x01, 0x02, 0x03, 0x04 };
      testSimpleSerializer<int32_t>(0x04030201, expected);
   }

   {
      const char expected[] = { '\xff', '\xff', '\xff', '\xff' };
      testSimpleSerializer<uint32_t>(-1, expected);
   }

   {
      const char expected[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
      testSimpleSerializer<int64_t>(0x0807060504030201LL, expected);
   }

   {
      const char expected[] = { 1, 2, 3, 4, 5, 6, 7, '\xff' };
      testSimpleSerializer<uint64_t>(0xff07060504030201ULL, expected);
   }
}

TEST(Serialization, pair)
{
   const char expected[] = {
      1, 0, 0, 0,
      1 };

   testSimpleSerializer(
         std::pair<int, bool>(1, 1),
         expected);
}

TEST(Serialization, tuple)
{
   const char expected[] = {
      1, 0, 0, 0,
      1,
      's' };

   testSimpleSerializer(
         std::tuple<int, bool, char>(1, 1, 's'),
         expected);
}

template<typename Ctx>
static void testPadFieldTo(Ctx& ctx, uint8_t& a, uint16_t& b, uint32_t& c)
{
   {
      PadFieldTo<Ctx> _(ctx, 3);
      ctx % a;
   }

   {
      PadFieldTo<Ctx> _(ctx, 2);
      ctx % b;
   }

   {
      PadFieldTo<Ctx> _(ctx, 8);
   }

   {
      PadFieldTo<Ctx> _(ctx, 2);
      ctx % c;
   }

   {
      PadFieldTo<Ctx> _(ctx, 8);
      ctx % c;
   }
}

TEST(Serialization, padFieldTo)
{
   const char expected[] = {
      1, 0, 0,
      1, 1,
      // field empty
      1, 1, 1, 1,
      1, 1, 1, 1, 0, 0, 0, 0, };

   uint8_t a = 0x01;
   uint16_t b = 0x0101;
   uint32_t c = 0x01010101;

   {
      Serializer ser;
      ::testPadFieldTo(ser, a, b, c);
      ASSERT_EQ(ser.size(), sizeof(expected));
   }

   {
      char outBuf[sizeof(expected)];

      Serializer ser(outBuf, sizeof(outBuf));
      ::testPadFieldTo(ser, a, b, c);
      ASSERT_TRUE(ser.good());
      ASSERT_TRUE(memoryEquals(outBuf, expected, sizeof(expected)));
   }

   {
      Deserializer des(expected, sizeof(expected));
      uint8_t a = 0;
      uint16_t b = 0;
      uint32_t c = 0;

      ::testPadFieldTo(des, a, b, c);
      ASSERT_TRUE(des.good());
      ASSERT_EQ(des.size(), sizeof(expected));
      ASSERT_EQ(a, 0x01);
      ASSERT_EQ(b, 0x0101);
      ASSERT_EQ(c, 0x01010101u);
   }

   {
      Deserializer des(expected, sizeof(expected) - 1);
      uint8_t a = 0;
      uint16_t b = 0;
      uint32_t c = 0;

      ::testPadFieldTo(des, a, b, c);
      ASSERT_FALSE(des.good());
   }
}

namespace {
enum class TestEnum1 { first = 1, second = -2, };
GCC_COMPAT_ENUM_CLASS_OPEQNEQ(TestEnum1)

struct EnumAsMod {
   auto operator()(TestEnum1& value)
      -> decltype(serdes::as<int64_t>(value))
   {
      return serdes::as<int64_t>(value);
   }

   auto operator()(const TestEnum1& value)
      -> decltype(serdes::as<int64_t>(value))
   {
      return serdes::as<int64_t>(value);
   }
};
}

template<>
struct SerializeAs<TestEnum1> {
   typedef int16_t type;
};

TEST(Serialization, serializeAs)
{
   {
      const char expected[] = {
         1, 0,
         '\xfe', '\xff', };

      testSimpleSerializer(std::make_tuple(TestEnum1::first, TestEnum1::second), expected);
   }

   {
      const char expected[] = { 1, 0, 0, 0, 0, 0, 0, 0, };

      TestEnum1 value = TestEnum1::first;
      testObject(value, expected, EnumAsMod());
   }
}

TEST(Serialization, rawString)
{
   {
      const char expected[] = {
         2, 0, 0, 0,
         'a', 'b', 0, };
      const char* input = expected + 4;

      {
         Serializer ser;
         ser % serdes::rawString(input, 2);
         ASSERT_EQ(ser.size(), sizeof(expected));
      }

      {
         char outBuf[sizeof(expected)];

         Serializer ser(outBuf, sizeof(outBuf));
         ser % serdes::rawString(input, 2);
         ASSERT_TRUE(ser.good());
         ASSERT_TRUE(memoryEquals(outBuf, expected, sizeof(expected)));
      }

      {
         Deserializer des(expected, sizeof(expected));
         const char* data = nullptr;
         unsigned length = 0;

         des % serdes::rawString(data, length);
         ASSERT_TRUE(des.good());
         ASSERT_EQ(des.size(), sizeof(expected));
         ASSERT_EQ(data, input);
         ASSERT_EQ(length, 2u);
      }

      // short length field
      {
         Deserializer des(expected, 1);
         const char* data = nullptr;
         unsigned length = 0;

         des % serdes::rawString(data, length);
         ASSERT_FALSE(des.good());
      }

      // short data
      {
         Deserializer des(expected, sizeof(expected) - 1);
         const char* data = nullptr;
         unsigned length = 0;

         des % serdes::rawString(data, length);
         ASSERT_FALSE(des.good());
      }
   }

   {
      const char expectedAligned[] = {
         2, 0, 0, 0,
         'a', 'b', 0, 0, };
      const char* input = expectedAligned + 4;

      {
         Serializer ser;
         ser % serdes::rawString(input, 2, 4);
         ASSERT_EQ(ser.size(), sizeof(expectedAligned));
      }

      {
         char outBuf[sizeof(expectedAligned)];

         Serializer ser(outBuf, sizeof(outBuf));
         ser % serdes::rawString(input, 2, 4);
         ASSERT_TRUE(ser.good());
         ASSERT_TRUE(memoryEquals(outBuf, expectedAligned, sizeof(expectedAligned)));
      }

      {
         Deserializer des(expectedAligned, sizeof(expectedAligned));
         const char* data = nullptr;
         unsigned length = 0;

         des % serdes::rawString(data, length, 4);
         ASSERT_TRUE(des.good());
         ASSERT_EQ(des.size(), sizeof(expectedAligned));
         ASSERT_EQ(data, input);
         ASSERT_EQ(length, 2u);
      }

      // short length field
      {
         Deserializer des(expectedAligned, 1);
         const char* data = nullptr;
         unsigned length = 0;

         des % serdes::rawString(data, length, 4);
         ASSERT_FALSE(des.good());
      }

      // short data
      {
         Deserializer des(expectedAligned, sizeof(expectedAligned) - 1);
         const char* data = nullptr;
         unsigned length = 0;

         des % serdes::rawString(data, length, 4);
         ASSERT_FALSE(des.good());
      }
   }

   {
      const char badTerminator[] = {
         2, 0, 0, 0,
         'a', 'b', 1, };

      Deserializer des(badTerminator, sizeof(badTerminator));
      const char* data = nullptr;
      unsigned length = 0;
      des % serdes::rawString(data, length);
      ASSERT_FALSE(des.good());
   }
}

namespace {
struct StringAlign4
{
   serdes::RawStringSer operator()(const std::string& s) { return serdes::stringAlign4(s); }
   serdes::StdString    operator()(      std::string& s) { return serdes::stringAlign4(s); }
};
}

TEST(Serialization, str)
{
   {
      const char* input = "test";
      const char expected[] = {
         4, 0, 0, 0,
         't', 'e', 's', 't', 0, };

      testSimpleSerializer(std::string(input), expected);
   }
   {
      const char* input = "test";
      const char expected[] = {
         4, 0, 0, 0,
         't', 'e', 's', 't', 0,
         0, 0, 0, };

      char outBuf[sizeof(expected)];

      {
         Serializer ser(outBuf, sizeof(expected));
         ser % serdes::rawString(input, strlen(input), 4);
         ASSERT_TRUE(ser.good());
         ASSERT_EQ(ser.size(), sizeof(expected));
         ASSERT_EQ(memcmp(outBuf, expected, sizeof(expected)), 0);
      }

      {
         Deserializer des(outBuf, sizeof(expected));
         const char* output = nullptr;
         unsigned outLen = 0;

         des % serdes::rawString(output, outLen, 4);
         ASSERT_TRUE(des.good());
         ASSERT_EQ(outLen, strlen(input));
         ASSERT_EQ(des.size(), sizeof(expected));
         ASSERT_EQ(strcmp(input, output), 0);
      }

      {
         Deserializer des(outBuf, sizeof(expected) - 1);
         const char* output = nullptr;
         unsigned outLen = 0;

         des % serdes::rawString(output, outLen, 4);
         ASSERT_FALSE(des.good());
      }

      {
         std::string str = input;
         testObject(str, expected, StringAlign4());
      }

      // one more, but with one byte of prefix. without the prefix, the result should still be the
      // same
      {
         const char expected[] = {
            1,
            4, 0, 0, 0,
            't', 'e', 's', 't', 0,
            0, 0, 0, };

         char outBuf[sizeof(expected)];

         {
            Serializer ser(outBuf, sizeof(expected));
            ser
               % char(1)
               % serdes::stringAlign4(input);

            ASSERT_TRUE(ser.good());
            ASSERT_EQ(ser.size(), sizeof(expected));
         }

         ASSERT_EQ(memcmp(outBuf, expected, sizeof(expected) ), 0);

         {
            Deserializer des(outBuf, sizeof(expected));
            const char* output = nullptr;
            unsigned outLen = 0;

            des.skip(1);
            des % serdes::rawString(output, outLen, 4);
            ASSERT_TRUE(des.good());
            ASSERT_EQ(des.size(), sizeof(expected));
            ASSERT_EQ(strcmp(input, output), 0);
         }

         {
            Deserializer des(outBuf, sizeof(expected) - 1);
            const char* output = nullptr;
            unsigned outLen = 0;

            des.skip(1);
            des % serdes::rawString(output, outLen, 4);
            ASSERT_FALSE(des.good());
         }
      }
   }
}

namespace {
struct HeapU32
{
   uint32_t value;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx % obj->value;
   }
};

inline Deserializer& operator%(Deserializer& des, std::unique_ptr<HeapU32>& ptr)
{
   ptr.reset(new HeapU32);
   return des % *ptr;
}
}

TEST(Serialization, backedPtr)
{
   const char expected[] = { 1, 0, 0, 0 };

   uint32_t value = 1;
   uint32_t* ptr = &value;

   {
      Serializer ser;
      ser % serdes::backedPtr(ptr, (const uint32_t&) value);
      ASSERT_EQ(ser.size(), sizeof(expected));
   }

   {
      char outBuf[sizeof(expected)];

      Serializer ser(outBuf, sizeof(outBuf));
      ser % serdes::backedPtr(ptr, (const uint32_t&) value);
      ASSERT_TRUE(ser.good());
      ASSERT_TRUE(memoryEquals(outBuf, expected, sizeof(expected)));
   }

   {
      char outBuf[sizeof(expected)];

      Serializer ser(outBuf, sizeof(outBuf));
      ser % serdes::backedPtr((const uint32_t*) ptr, (const uint32_t&) value);
      ASSERT_TRUE(ser.good());
      ASSERT_TRUE(memoryEquals(outBuf, expected, sizeof(expected)));
   }

   {
      Deserializer des(expected, sizeof(expected));
      uint32_t* ptr = nullptr;

      des % serdes::backedPtr(ptr, value);
      ASSERT_TRUE(des.good());
      ASSERT_EQ(des.size(), sizeof(expected));
      ASSERT_EQ(*ptr, 1u);
      ASSERT_EQ(ptr, &value);
   }

   {
      Deserializer des(expected, sizeof(expected));
      const uint32_t* ptr = nullptr;

      des % serdes::backedPtr(ptr, value);
      ASSERT_TRUE(des.good());
      ASSERT_EQ(des.size(), sizeof(expected));
      ASSERT_EQ(*ptr, 1u);
      ASSERT_EQ(ptr, &value);
   }

   {
      Deserializer des(expected, sizeof(expected));
      HeapU32 value{1};
      HeapU32* ptr = &value;
      std::unique_ptr<HeapU32> backing;

      des % serdes::backedPtr(ptr, backing);
      ASSERT_TRUE(des.good());
      ASSERT_EQ(des.size(), sizeof(expected));
      ASSERT_EQ(ptr->value, 1u);
      ASSERT_EQ(ptr, backing.get());
   }

   {
      Deserializer des(expected, sizeof(expected) - 1);
      uint32_t* ptr = nullptr;

      des % serdes::backedPtr(ptr, value);
      ASSERT_FALSE(des.good());
   }
}

TEST(Serialization, rawBlock)
{
   const char expected[] = { 1, 0, 0, 0 };

   {
      Serializer ser;
      ser % serdes::rawBlock(expected, 4);
      ASSERT_EQ(ser.size(), sizeof(expected));
   }

   {
      char outBuf[sizeof(expected)];

      Serializer ser(outBuf, sizeof(outBuf));
      ser % serdes::rawBlock(expected, 4);
      ASSERT_EQ(ser.size(), sizeof(expected));
      ASSERT_TRUE(ser.good());
      ASSERT_TRUE(memoryEquals(outBuf, expected, sizeof(expected)));
   }

   {
      Deserializer des(expected, sizeof(expected));
      const char* data;
      uint32_t size = 4;

      des % serdes::rawBlock(data, size);
      ASSERT_TRUE(des.good());
      ASSERT_EQ(des.size(), sizeof(expected));
      ASSERT_EQ(data, expected);
   }

   {
      Deserializer des(expected, sizeof(expected) - 1);
      const char* data;
      uint32_t size = 4;

      des % serdes::rawBlock(data, size);
      ASSERT_FALSE(des.good());
   }
}

TEST(Serialization, block)
{
   // wrapping size_t invalidates serializer
   {
      char buffer;
      Serializer ser(&buffer, 1);
      ser % char(1);
      ser.putBlock(nullptr, size_t(-1));
      ASSERT_FALSE(ser.good());
   }
   {
      char buffer;
      Deserializer des(&buffer, 1);
      des % buffer;
      des.getBlock(nullptr, size_t(-1));
      ASSERT_FALSE(des.good());
   }
   {
      char buffer;
      Deserializer des(&buffer, 1);
      des % buffer;
      des.skip(size_t(-1));
      ASSERT_FALSE(des.good());
   }

   // going past end invalidates serializer
   {
      char buffer;
      Serializer ser(&buffer, 1);
      ser % uint32_t(1);
      ASSERT_FALSE(ser.good());
   }
   {
      char buffer;
      Deserializer des(&buffer, 1);
      des % buffer;
      des.skip(1);
      ASSERT_FALSE(des.good());
   }

   // skipping nothing does nothing
   {
      char buffer;
      Serializer ser(&buffer, 1);
      ASSERT_EQ(ser.size(), 0u);
      ASSERT_TRUE(ser.good());
      ser.skip(0);
      ASSERT_EQ(ser.size(), 0u);
      ASSERT_TRUE(ser.good());
   }
   {
      char buffer;
      Deserializer des(&buffer, 1);
      des % buffer;
      ASSERT_EQ(des.size(), 1u);
      ASSERT_TRUE(des.good());
      des.skip(0);
      ASSERT_EQ(des.size(), 1u);
      ASSERT_TRUE(des.good());
   }

   // blocks zeroes memory
   {
      char buffer[128];

      for (unsigned i = 0; i < sizeof(buffer); i++)
         buffer[i] = 1;

      Serializer ser(buffer, sizeof(buffer));
      ser.skip(sizeof(buffer));
      ASSERT_EQ(ser.size(), sizeof(buffer));
      ASSERT_TRUE(ser.good());

      for (unsigned i = 0; i < sizeof(buffer); i++)
         ASSERT_EQ(buffer[i], 0);
   }
}

namespace {
struct NonPrimitive
{
   uint32_t data;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx % obj->data;
   }

   friend bool operator==(NonPrimitive a, NonPrimitive b)
   {
      return a.data == b.data;
   }
};
}

TEST(Serialization, nonPrimitive)
{
   NonPrimitive value{1};
   const char expected[] = { 1, 0, 0, 0 };

   testSimpleSerializer<NonPrimitive>(value, expected);
}

namespace {
template<bool HasLength>
struct CollectionItem
{
   uint8_t value;

   CollectionItem() = default;

   CollectionItem(uint8_t value) : value(value) {}

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx % obj->value;
   }

   friend bool operator<(CollectionItem a, CollectionItem b)
   {
      return a.value < b.value;
   }

   friend bool operator==(CollectionItem a, CollectionItem b)
   {
      return a.value == b.value;
   }
};

typedef CollectionItem<true> ItemWithLength;
typedef CollectionItem<false> ItemWithoutLength;
}

template<>
struct ListSerializationHasLength<ItemWithLength> : std::true_type {};

template<>
struct MapSerializationHasLength<uint8_t, ItemWithLength> : std::true_type {};

template<>
struct ListSerializationHasLength<ItemWithoutLength> : std::false_type {};

template<>
struct MapSerializationHasLength<uint8_t, ItemWithoutLength> : std::false_type {};

template<typename CollectionWithLength, typename CollectionWithoutLength, size_t N>
static void testCollection(const char (&content)[N],
   const typename CollectionWithLength::value_type& withLength,
   const typename CollectionWithoutLength::value_type& withoutLength)
{
   SCOPED_TRACE(__PRETTY_FUNCTION__);

   // without length, empty
   {
      std::vector<char> expected = {0, 0, 0, 0};
      CollectionWithoutLength empty;
      testObject(empty, &expected.front(), &expected.back() + 1);
   }

   // with length, empty
   {
      std::vector<char> expected = {
         8, 0, 0, 0,
         0, 0, 0, 0};
      CollectionWithLength empty;
      testObject(empty, &expected.front(), &expected.back() + 1);
   }

   // without length, value
   {
      std::vector<char> expected = {1, 0, 0, 0};
      expected.insert(expected.end(), content, content + N);
      CollectionWithoutLength collection{{withoutLength}};
      testObject(collection, &expected.front(), &expected.back() + 1);
   }

   // with length, value
   {
      // 64 to ensure that the result is well within the range of signed char
      static_assert(8 + N < 64);

      std::vector<char> expected = {
         8 + N, 0, 0, 0,
         1, 0, 0, 0};
      expected.insert(expected.end(), content, content + N);
      CollectionWithLength collection{{withLength}};
      testObject(collection, &expected.front(), &expected.back() + 1);
   }

   // with incorrect length
   {
      std::vector<char> expected = {
         7, 0, 0, 0,
         0, 0, 0, 0};

      CollectionWithLength value;
      Deserializer des(&expected[0], expected.size());
      des % value;
      ASSERT_FALSE(des.good());
   }

   // with corrupted length
   {
      std::vector<char> expected = {7, 0, 0};

      CollectionWithLength value;
      Deserializer des(&expected[0], expected.size());
      des % value;
      ASSERT_FALSE(des.good());
   }

   // with length, corrupted size
   {
      std::vector<char> expected = {
         8, 0, 0, 0,
         1, 0, 0};

      CollectionWithLength value;
      Deserializer des(&expected[0], expected.size());
      des % value;
      ASSERT_FALSE(des.good());
   }

   // without length, corrupted size
   {
      std::vector<char> expected = {1, 0, 0};

      CollectionWithoutLength value;
      Deserializer des(&expected[0], expected.size());
      des % value;
      ASSERT_FALSE(des.good());
   }
}

TEST(Serialization, collections)
{
   {
      const char expected[] = {1};
      testCollection<
         std::list<ItemWithLength>,
         std::list<ItemWithoutLength>>(expected, 1, 1);
      testCollection<
         std::vector<ItemWithLength>,
         std::vector<ItemWithoutLength>>(expected, 1, 1);
      testCollection<
         std::set<ItemWithLength>,
         std::set<ItemWithoutLength>>(expected, 1, 1);
   }
   {
      const char expected[] = {1, 2};
      testCollection<
         std::map<uint8_t, ItemWithLength>,
         std::map<uint8_t, ItemWithoutLength>>(expected, {1, 2}, {1, 2});
   }
}

namespace {
template<typename As>
struct AtomicMod
{
   template<typename T>
   serdes::AtomicAsSer<T, As> operator()(const Atomic<T>& t) const
   {
      return serdes::atomicAs<As>(t);
   }

   template<typename T>
   serdes::AtomicAsDes<T, As> operator()(Atomic<T>& t) const
   {
      return serdes::atomicAs<As>(t);
   }
};

struct AtomicEq
{
   template<typename T>
   bool operator()(const Atomic<T>& a, const Atomic<T>& b) const
   {
      return a.read() == b.read();
   }
};
}

TEST(Serialization, atomic)
{
   const char expected[] = {1, 0, 0, 0};
   Atomic<uint32_t> value(1);
   testObject(value, expected, AtomicMod<uint32_t>(), AtomicEq());
}

namespace {
struct BaseType
{
   uint8_t a;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx % obj->a;
   }
};

struct DerivedType : BaseType
{
   uint32_t b;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % serdes::base<BaseType>(obj)
         % obj->b;
   }

   friend bool operator==(DerivedType a, DerivedType b)
   {
      return a.a == b.a && a.b == b.b;
   }
};
}

TEST(Serialization, base)
{
   DerivedType value;
   value.a = 1;
   value.b = 2;

   const char expected[] = {
      1,
      2, 0, 0, 0
   };

   testSimpleSerializer<DerivedType>(value, expected);
}

template<template<typename...> class Collection, size_t N>
static void testStringCollection(const char (&expected)[N])
{
   SCOPED_TRACE(__PRETTY_FUNCTION__);

   // empty
   {
      const char expected[] = {
         8, 0, 0, 0,
         0, 0, 0, 0};
      Collection<std::string> empty;
      testSimpleSerializer(empty, expected);
   }

   // values b, a
   {
      Collection<std::string> values;
      values.insert(values.end(), "b");
      values.insert(values.end(), "a");
      testSimpleSerializer(values, expected);
   }

   // corrupt length field
   {
      const char input[] = { 0 };
      Deserializer des(input, sizeof(input));
      Collection<std::string> value;
      des % value;
      ASSERT_FALSE(des.good());
   }

   // corrupt size field
   {
      const char input[] = {
         8, 0, 0, 0,
         0, 0, 0};
      Deserializer des(input, sizeof(input));
      Collection<std::string> value;
      des % value;
      ASSERT_FALSE(des.good());
   }

   // too many items
   {
      const char input[] = {
         8, 0, 0, 0,
         1, 0, 0, 0};
      Deserializer des(input, sizeof(input));
      Collection<std::string> value;
      des % value;
      ASSERT_FALSE(des.good());
   }

   // too much length
   {
      const char input[] = {
         9, 0, 0, 0,
         0, 0, 0, 0};
      Deserializer des(input, sizeof(input));
      Collection<std::string> value;
      des % value;
      ASSERT_FALSE(des.good());
   }

   // dump not properly terminated
   {
      const char input[] = {
         10, 0, 0, 0,
         1, 0, 0, 0,
         'a', 'a'};
      Deserializer des(input, sizeof(input));
      Collection<std::string> value;
      des % value;
      ASSERT_FALSE(des.good());
   }
}

TEST(Serialization, stringCollections)
{
   {
      const char expected[] = {
         12, 0, 0, 0,
         2, 0, 0, 0,
         'b', 0, 'a', 0};

      testStringCollection<std::list>(expected);
      testStringCollection<std::vector>(expected);
   }
   {
      const char expected[] = {
         12, 0, 0, 0,
         2, 0, 0, 0,
         'a', 0, 'b', 0};

      testStringCollection<std::set>(expected);
   }
}
