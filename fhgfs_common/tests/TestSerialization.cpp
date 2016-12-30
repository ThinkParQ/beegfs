#include "TestSerialization.h"

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

namespace {

namespace data {

namespace EntryInfo {
const class EntryInfo value(
   NumNodeID(127),
   "0-1-2",
   "1-2-3",
   "testFile",
   DirEntryType_REGULARFILE,
   127);

#define TEST_SER_ENTRYINFO_RAW \
   /* entryType */ \
   2, 0, 0, 0, \
   /* featureFlags */ \
   127, 0, 0, 0, \
   /* parentEntryID */ \
   5, 0, 0, 0, \
   '0', '-', '1', '-', '2', 0, \
   0, 0, \
   /* entryID */ \
   5, 0, 0, 0, \
   '1', '-', '2', '-', '3', 0, \
   0, 0, \
   /* fileName */ \
   8, 0, 0, 0, \
   't', 'e', 's', 't', 'F', 'i', 'l', 'e', 0, \
   0, 0, 0, \
   /* ownerNodeID */ \
   127, 0, 0, 0, \
   /* padding */ \
   0, 0

const char expected[] = {
   TEST_SER_ENTRYINFO_RAW
};

bool eq(const class EntryInfo& info, const class EntryInfo& outInfo)
{
   return info.getOwnerNodeID() == outInfo.getOwnerNodeID()
      && info.getParentEntryID() == outInfo.getParentEntryID()
      && info.getEntryID() == outInfo.getEntryID()
      && info.getFileName() == outInfo.getFileName()
      && info.getEntryType() == outInfo.getEntryType()
      && info.getFeatureFlags() == outInfo.getFeatureFlags();
}

}

namespace EntryInfoWithDepth {
const class EntryInfoWithDepth value(
   NumNodeID(127),
   "0-1-2",
   "1-2-3",
   "testFile",
   DirEntryType_REGULARFILE,
   127,
   42);

#define TEST_SER_ENTRYINFOWITHDEPTH_RAW \
   TEST_SER_ENTRYINFO_RAW, \
   /* depth */ \
   42, 0, 0, 0

const char expected[] = {
   TEST_SER_ENTRYINFOWITHDEPTH_RAW
};

bool eq(const class EntryInfoWithDepth& info, const class EntryInfoWithDepth& outInfo)
{
   return data::EntryInfo::eq(info, outInfo)
      && info.getEntryDepth() == outInfo.getEntryDepth();
}

}

namespace Raid0Pattern {
UInt16Vector makePattern()
{
   UInt16Vector result;
   result.push_back(0x0104);
   result.push_back(0x0302);
   return result;
}

const class Raid0Pattern value(0xfffe, makePattern(), 4);

#define TEST_SER_RAID0PATTERN_RAW \
      /* header */ \
      /* -> pattern length */ \
      28, 0, 0, 0, \
      /* -> pattern type */ \
      StripePatternType_Raid0, 0, 0, 0, \
      /* -> chunk size */ \
      '\xfe', '\xff', 0, 0, \
      /* pattern data */ \
      /* -> defaultNumTargets */ \
      4, 0, 0, 0, \
      /* -> stripe target ids */ \
      12, 0, 0, 0, \
      2, 0, 0, 0, \
      4, 1, \
      2, 3

const char expected[] = {
   TEST_SER_RAID0PATTERN_RAW
};

}

namespace NicAddress {
static bool eq(const struct NicAddress& a, const struct NicAddress& b)
{
   return a.ipAddr.s_addr == b.ipAddr.s_addr
      && ::strncmp(a.name, b.name, IFNAMSIZ) == 0
      && a.nicType == b.nicType;
}

static bool listEq(const NicAddressList& a, const NicAddressList& b)
{
   return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), eq);
}
}

}

}

void TestSerialization::testByteswap()
{
   CPPUNIT_ASSERT(byteswap16(0x0102) == 0x0201);
   CPPUNIT_ASSERT(byteswap32(0x01020304) == 0x04030201);
   CPPUNIT_ASSERT(byteswap64(0x0102030405060708ULL) == 0x0807060504030201ULL);
}

void TestSerialization::testStr()
{
   const char* input = "test";
   const char expected[] = {
      4, 0, 0, 0,
      't', 'e', 's', 't', 0,
   };

   char outBuf[sizeof(expected)];

   {
      Serializer ser(outBuf, sizeof(expected));
      ser % serdes::rawString(input, strlen(input));
      CPPUNIT_ASSERT(ser.good());
      CPPUNIT_ASSERT(ser.size() == sizeof(expected));
      CPPUNIT_ASSERT(memcmp(outBuf, expected, sizeof(expected) ) == 0);
   }

   {
      Deserializer des(outBuf, sizeof(expected));
      const char* output = nullptr;
      unsigned outLen = 0;

      des % serdes::rawString(output, outLen);
      CPPUNIT_ASSERT(des.good());
      CPPUNIT_ASSERT(outLen == strlen(input) );
      CPPUNIT_ASSERT(des.size() == sizeof(expected) );
      CPPUNIT_ASSERT(strcmp(input, output) == 0);
   }

   {
      Deserializer des(outBuf, sizeof(expected) - 1);
      const char* output = nullptr;
      unsigned outLen = 0;

      des % serdes::rawString(output, outLen);
      CPPUNIT_ASSERT(!des.good());
   }

   testPrimitiveType(std::string(input), expected);
}

namespace {
struct StringAlign4
{
   serdes::RawStringSer operator()(const std::string& s) { return serdes::stringAlign4(s); }
   serdes::StdString    operator()(      std::string& s) { return serdes::stringAlign4(s); }
};
}

void TestSerialization::testStrAlign4()
{
   const char* input = "test";
   const char expected[] = {
      4, 0, 0, 0,
      't', 'e', 's', 't', 0,
      0, 0, 0,
   };

   char outBuf[sizeof(expected)];

   {
      Serializer ser(outBuf, sizeof(expected));
      ser % serdes::rawString(input, strlen(input), 4);
      CPPUNIT_ASSERT(ser.good());
      CPPUNIT_ASSERT(ser.size() == sizeof(expected));
      CPPUNIT_ASSERT(memcmp(outBuf, expected, sizeof(expected) ) == 0);
   }

   {
      Deserializer des(outBuf, sizeof(expected));
      const char* output = nullptr;
      unsigned outLen = 0;

      des % serdes::rawString(output, outLen, 4);
      CPPUNIT_ASSERT(des.good());
      CPPUNIT_ASSERT(outLen == strlen(input) );
      CPPUNIT_ASSERT(des.size() == sizeof(expected) );
      CPPUNIT_ASSERT(strcmp(input, output) == 0);
   }

   {
      Deserializer des(outBuf, sizeof(expected) - 1);
      const char* output = nullptr;
      unsigned outLen = 0;

      des % serdes::rawString(output, outLen, 4);
      CPPUNIT_ASSERT(!des.good());
   }

   {
      std::string str = input;
      testObject(str, expected, StringAlign4(), std::equal_to<std::string>());
   }

   // one moere, but with one byte of prefix. without the prefix, the result should still be the
   // same
   {
      const char expected[] = {
         1,
         4, 0, 0, 0,
         't', 'e', 's', 't', 0,
         0, 0, 0,
      };

      char outBuf[sizeof(expected)];

      {
         Serializer ser(outBuf, sizeof(expected));
         ser
            % char(1)
            % serdes::stringAlign4(input);

         CPPUNIT_ASSERT(ser.good());
         CPPUNIT_ASSERT(ser.size() == sizeof(expected));
      }

      CPPUNIT_ASSERT(memcmp(outBuf, expected, sizeof(expected) ) == 0);

      {
         Deserializer des(outBuf, sizeof(expected));
         const char* output = nullptr;
         unsigned outLen = 0;

         des.skip(1);
         des % serdes::rawString(output, outLen, 4);
         CPPUNIT_ASSERT(des.good());
         CPPUNIT_ASSERT(des.size() == sizeof(expected));
         CPPUNIT_ASSERT(strcmp(input, output) == 0);
      }

      {
         Deserializer des(outBuf, sizeof(expected) - 1);
         const char* output = nullptr;
         unsigned outLen = 0;

         des.skip(1);
         des % serdes::rawString(output, outLen, 4);
         CPPUNIT_ASSERT(!des.good());
      }
   }
}

void TestSerialization::testChar()
{
   const char expected[] = { 'X' };

   testPrimitiveType<char>('X', expected);
}

void TestSerialization::testBool()
{
   const char expectedFalse[] = { 0 };
   const char expectedTrue[] = { 1 };

   testPrimitiveType<bool>(false, expectedFalse);
   testPrimitiveType<bool>(true, expectedTrue);
}

void TestSerialization::testShort()
{
   const char expected[] = { 2, 1 };

   testPrimitiveType<short>(258, expected);
}

void TestSerialization::testUShort()
{
   const char expected[] = { '\xff', '\xff' };

   testPrimitiveType<unsigned short>(-1, expected);
}

void TestSerialization::testInt()
{
   const char expected[] = { 0x01, 0x02, 0x03, 0x04 };

   testPrimitiveType<int>(0x04030201, expected);
}

void TestSerialization::testUInt()
{
   const char expected[] = { '\xff', '\xff', '\xff', '\xff' };

   testPrimitiveType<unsigned int>(-1, expected);
}

void TestSerialization::testUInt8()
{
   const char expected[] = { '\xff' };

   testPrimitiveType<uint8_t>(255, expected);
}

void TestSerialization::testUInt16()
{
   const char expected[] = { 2, 1 };

   testPrimitiveType<uint16_t>(258, expected);
}

void TestSerialization::testInt64()
{
   const char expected[] = { 1, 2, 3, 4, 5, 6, 7, 8 };

   testPrimitiveType<int64_t>(0x0807060504030201LL, expected);
}

void TestSerialization::testUInt64()
{
   const char expected[] = { 1, 2, 3, 4, 5, 6, 7, '\xff' };

   testPrimitiveType<uint64_t>(0xff07060504030201ULL, expected);
}

void TestSerialization::testTuple()
{
   const char expected[] = {
      1, 0, 0, 0,
      1,
      4, 0, 0, 0, 't', 'e', 's', 't', 0,
      's' };

   std::tuple<int, bool, std::string, char> value(1, 1, "test", 's');

   testPrimitiveType(value, expected);
}

void TestSerialization::testNumNodeID()
{
   const char expected[] = { 1, 2, 3, 4 };

   NumNodeID id(0x04030201);

   testPrimitiveType(id, expected);
}

void TestSerialization::testPath()
{
   const char expected[] = {
      4, 0, 0, 0,
      't', 'e', 's', 't', 0,
   };

   testPrimitiveType<Path>(Path("test"), expected);
}

void TestSerialization::testStorageTargetInfo()
{
   const char expected[] = {
      1, 2,
      1, 0, 0, 0, 'x', 0, 0, 0,
      1, 2, 3, 4, 5, 6, 7, 8,
      2, 3, 4, 5, 6, 7, 8, 9,
      3, 4, 5, 6, 7, 8, 9, 0,
      4, 5, 6, 7, 8, 9, 0, 1,
      TargetConsistencyState_NEEDS_RESYNC
   };

   StorageTargetInfo info(0x0201, "x", 0x0807060504030201ULL, 0x0908070605040302ULL,
      0x0009080706050403ULL, 0x0100090807060504ULL, TargetConsistencyState_NEEDS_RESYNC);

   testPrimitiveType(info, expected);


   std::vector<char> expectedList;
   const char listHeader[] = { 1, 0, 0, 0 };

   expectedList.insert(expectedList.end(), listHeader, listHeader + sizeof(listHeader) );
   expectedList.insert(expectedList.end(), expected, expected + sizeof(expected) );

   StorageTargetInfo listData[] = { info };

   testCollection<StorageTargetInfoList>(listData, expectedList,
      std::equal_to<StorageTargetInfo>() );
}

void TestSerialization::testQuotaData()
{
   const char expected[] = {
      4, 0, 0, 0, 0, 0, 0, 0,
      3, 0, 0, 0, 0, 0, 0, 0,
      2, 0, 0, 0,
      1, 0, 0, 0,
      0,
   };

   QuotaData data(2, QuotaDataType_USER);
   data.setQuotaData(4, 3);
   data.setIsValid(false);

   struct ops
   {
      static bool eq(const QuotaData& a, const QuotaData& b)
      {
         return a.getSize() == b.getSize()
            && a.getInodes() == b.getInodes()
            && a.getID() == b.getID()
            && a.getType() == b.getType()
            && a.isValid() == b.isValid();
      }

      static bool peq(const std::pair<const unsigned, QuotaData>& a,
         const std::pair<const unsigned, QuotaData>& b)
      {
         return a.first == b.first && eq(a.second, b.second);
      }

      static bool ppeq(const std::pair<const uint16_t, QuotaDataMap>& a,
         const std::pair<const uint16_t, QuotaDataMap>& b)
      {
         return a.first == b.first
            && a.second.size() == b.second.size()
            && std::equal(a.second.begin(), a.second.end(), b.second.begin(), peq);
      }
   };

   testObject(data, expected, NoModifier(), ops::eq);

   {
      const char listHeader[] = { 1, 0, 0, 0 };
      std::vector<char> expectedList;

      expectedList.insert(expectedList.end(), listHeader, listHeader + sizeof(listHeader) );
      expectedList.insert(expectedList.end(), expected, expected + sizeof(expected) );

      QuotaData listData[] = { data };

      testCollection<QuotaDataList>(listData, expectedList, ops::eq);

      std::pair<const unsigned, QuotaData> mapData[] = { std::make_pair(data.getID(), data) };

      testCollection<QuotaDataMap>(mapData, expectedList, ops::peq);

      const char mapPrefix[] = {
         70, 0, 0, 0,
         2, 0, 0, 0,
      };

      std::vector<char> expectedMap;

      expectedMap.insert(expectedMap.end(), mapPrefix, mapPrefix + sizeof(mapPrefix) );
      expectedMap.push_back(0);
      expectedMap.push_back(0);
      expectedMap.insert(expectedMap.end(), listHeader, listHeader + sizeof(listHeader) );
      expectedMap.insert(expectedMap.end(), expected, expected + sizeof(expected) );
      expectedMap.push_back(1);
      expectedMap.push_back(0);
      expectedMap.insert(expectedMap.end(), listHeader, listHeader + sizeof(listHeader) );
      expectedMap.insert(expectedMap.end(), expected, expected + sizeof(expected) );

      std::pair<const uint16_t, QuotaDataMap> mapForTargetData[] = {
         std::make_pair(0, QuotaDataMap() ),
         std::make_pair(1, QuotaDataMap() ),
      };

      mapForTargetData[0].second.insert(std::make_pair(data.getID(), data) );
      mapForTargetData[1].second.insert(std::make_pair(data.getID(), data) );

      testCollection<QuotaDataMapForTarget>(mapForTargetData, expectedMap, ops::ppeq);
   }
}

void TestSerialization::testBitStore()
{
   {
      const char expected[] = {
         32, 0, 0, 0, 0, 0, 0, 0,
         '\x81', 0, 0, 0, 0, 0, 0, 0,
      };

      BitStore data(8);
      data.setBit(0, true);
      data.setBit(7, true);

      testObject(data, expected);
   }

   {
      const char expected[] = {
         32, 0, 0, 0, 0, 0, 0, 0,
         1, 0, 0, '\x80', 0, 0, 0, 0,
      };

      BitStore data(32);
      data.setBit(0, true);
      data.setBit(31, true);

      testObject(data, expected);
   }

   {
      const char expected[] = {
         64, 0, 0, 0, 0, 0, 0, 0,
         1, 0, 0, 0, 1, 0, 0, 0,
      };

      BitStore data(33);
      data.setBit(0, true);
      data.setBit(32, true);

      testObject(data, expected);
   }

   {
      const char expected[] = {
         64, 0, 0, 0, 0, 0, 0, 0,
         1, 0, 0, 0, 0, 0, 0, '\x80',
      };

      BitStore data(64);
      data.setBit(0, true);
      data.setBit(63, true);

      testObject(data, expected);
   }
}

void TestSerialization::testPathInfo()
{
   {
      const char expectedOld[] = {
         0, 0, 0, 0
      };

      PathInfo old;

      testObject(old, expectedOld);
   }

   {
      const char expected[] = {
         1, 0, 0, 0,
         42, 0, 0, 0,
         5, 0, 0, 0, 's', 'n', 'a', 'f', 'u', 0, 0, 0,
      };

      PathInfo data(42, "snafu", PATHINFO_FEATURE_ORIG);

      testObject(data, expected);
   }
}

namespace {
struct StatDataModify
{
   StatDataFormat format;

   StatDataModify(StatDataFormat format) : format(format) {}

   StatData::SerializeAs<StatData*> operator()(StatData& sd)
   { return sd.serializeAs(format); }

   StatData::SerializeAs<const StatData*> operator()(const StatData& sd)
   { return sd.serializeAs(format); }
};
}

void TestSerialization::testStatData()
{
   SettableFileAttribs attribs = {
      20, // mode
      1, // uid
      3, // gid,
      4, // mtime
      5, // atime
   };
   StatData data(6, &attribs, 7, 8, 9, 10);

   data.setTargetChunkBlocks(1, 0x300000000ULL, 2);
   data.setTargetChunkBlocks(0, 0x400000000ULL, 2);
   data.setSparseFlag();

   // these are nasty hacks around the fact the StatData serialization does *not* round-trip.
   // fields that are expected to not round-trip are set to match the input data.
   struct ops
   {
      static bool eq(StatData& a, StatData& b, StatDataFormat fmt)
      {
         return
            (
               fmt & (StatDataFormat_Flag_HasFlags | StatDataFormat_Flag_IsNet)
               ? a.flags == b.flags
                  && a.settableFileAttribs.mode == b.settableFileAttribs.mode
                  && (fmt & StatDataFormat_Flag_IsNet
                        ? true // numBlocks comparison is spurious
                        : a.getIsSparseFile()
                        ? a.chunkBlocksVec == b.chunkBlocksVec
                        : true)
               : true)
            && a.creationTimeSecs == b.creationTimeSecs
            && a.settableFileAttribs.lastAccessTimeSecs == b.settableFileAttribs.lastAccessTimeSecs
            && a.settableFileAttribs.modificationTimeSecs
               == b.settableFileAttribs.modificationTimeSecs
            && a.attribChangeTimeSecs == b.attribChangeTimeSecs
            && (!(fmt & StatDataFormat_Flag_DiskDirInode)
                  ? a.fileSize == b.fileSize
                     && a.nlink == b.nlink
                     && a.contentsVersion == b.contentsVersion
                  : true)
            && a.settableFileAttribs.userID == b.settableFileAttribs.userID
            && a.settableFileAttribs.groupID == b.settableFileAttribs.groupID
            && (!(fmt & StatDataFormat_Flag_HasFlags)
                  ? a.settableFileAttribs.mode == b.settableFileAttribs.mode
                  : true);
      }

      static bool eq1(StatData a, StatData b)
      {
         return eq(a, b, StatDataFormat_NET);
      }

      static bool eq2(StatData a, StatData b)
      {
         return eq(a, b, StatDataFormat_FILEINODE);
      }

      static bool eq3(StatData a, StatData b)
      {
         return eq(a, b, StatDataFormat_DENTRYV4);
      }

      static bool eq4(StatData a, StatData b)
      {
         return eq(a, b, StatDataFormat_DIRINODE);
      }

      static bool eq5(StatData a, StatData b)
      {
         return eq(a, b, StatDataFormat_DIRINODE_NOFLAGS);
      }
   };

   // hasFlags, isNet
   {
      const char expected[] = {
         1, 0, 0, 0,
         20, 0, 0, 0,
         0, 0, 0, 0, 7, 0, 0, 0,
         7, 0, 0, 0, 0, 0, 0, 0,
         5, 0, 0, 0, 0, 0, 0, 0,
         4, 0, 0, 0, 0, 0, 0, 0,
         8, 0, 0, 0, 0, 0, 0, 0,
         6, 0, 0, 0, 0, 0, 0, 0,
         9, 0, 0, 0,
         10, 0, 0, 0,
         1, 0, 0, 0,
         3, 0, 0, 0
      };

      testObject(data, expected, StatDataModify(StatDataFormat_NET), ops::eq1);
   }

   // hasFlags
   {
      const char expected[] = {
         1, 0, 0, 0,
         20, 0, 0, 0,
         24, 0, 0, 0,
            2, 0, 0, 0,
            0, 0, 0, 0, 4, 0, 0, 0,
            0, 0, 0, 0, 3, 0, 0, 0,
         7, 0, 0, 0, 0, 0, 0, 0,
         5, 0, 0, 0, 0, 0, 0, 0,
         4, 0, 0, 0, 0, 0, 0, 0,
         8, 0, 0, 0, 0, 0, 0, 0,
         6, 0, 0, 0, 0, 0, 0, 0,
         9, 0, 0, 0,
         10, 0, 0, 0,
         1, 0, 0, 0,
         3, 0, 0, 0
      };

      testObject(data, expected, StatDataModify(StatDataFormat_FILEINODE), ops::eq2);
   }

   // (all modifiers unset)
   {
      const char expected[] = {
         7, 0, 0, 0, 0, 0, 0, 0,
         5, 0, 0, 0, 0, 0, 0, 0,
         4, 0, 0, 0, 0, 0, 0, 0,
         8, 0, 0, 0, 0, 0, 0, 0,
         6, 0, 0, 0, 0, 0, 0, 0,
         9, 0, 0, 0,
         10, 0, 0, 0,
         1, 0, 0, 0,
         3, 0, 0, 0,
         20, 0, 0, 0,
      };

      testObject(data, expected, StatDataModify(StatDataFormat_DENTRYV4), ops::eq3);
   }

   // isDiskDirInode, hasFlags
   {
      const char expected[] = {
         1, 0, 0, 0,
         20, 0, 0, 0,
         24, 0, 0, 0,
            2, 0, 0, 0,
            0, 0, 0, 0, 4, 0, 0, 0,
            0, 0, 0, 0, 3, 0, 0, 0,
         7, 0, 0, 0, 0, 0, 0, 0,
         5, 0, 0, 0, 0, 0, 0, 0,
         4, 0, 0, 0, 0, 0, 0, 0,
         8, 0, 0, 0, 0, 0, 0, 0,
         1, 0, 0, 0,
         3, 0, 0, 0
      };

      testObject(data, expected, StatDataModify(StatDataFormat_DIRINODE), ops::eq4);
   }

   // isDiskDirInode
   {
      const char expected[] = {
         7, 0, 0, 0, 0, 0, 0, 0,
         5, 0, 0, 0, 0, 0, 0, 0,
         4, 0, 0, 0, 0, 0, 0, 0,
         8, 0, 0, 0, 0, 0, 0, 0,
         1, 0, 0, 0,
         3, 0, 0, 0,
         20, 0, 0, 0,
      };

      testObject(data, expected, StatDataModify(StatDataFormat_DIRINODE_NOFLAGS), ops::eq5);
   }
}

void TestSerialization::testUInt8Collections()
{
   const char expected[] = {
      12, 0, 0, 0,
      4, 0, 0, 0,
      1, 2, 3, 4,
   };

   uint8_t data[] = { 1, 2, 3, 4 };

   testCollection<UInt8List>(data, expected);
}

void TestSerialization::testUInt16Collections()
{
   const char expected[] = {
      12, 0, 0, 0,
      2, 0, 0, 0,
      1, 2,
      2, 3,
   };

   uint16_t data[] = { 0x0201, 0x0302 };

   testCollection<UInt16List>(data, expected);
   testCollection<UInt16Vector>(data, expected);
}

void TestSerialization::testUInt64Collections()
{
   const char expected[] = {
      24, 0, 0, 0,
      2, 0, 0, 0,
      1, 2, 3, 4, 5, 6, 7, 8,
      2, 3, 4, 5, 6, 7, 8, 9,
   };

   uint64_t data[] = { 0x0807060504030201ULL, 0x0908070605040302ULL };

   testCollection<UInt64Vector>(data, expected);
}

void TestSerialization::testInt64Collections()
{
   const char expected[] = {
      24, 0, 0, 0,
      2, 0, 0, 0,
      1, 2, 3, 4, 5, 6, 7, 8,
      2, 3, 4, 5, 6, 7, 8, 9,
   };

   int64_t data[] = { 0x0807060504030201LL, 0x0908070605040302LL };

   testCollection<Int64List>(data, expected);
}

void TestSerialization::testCharVector()
{
   const char expected[] = {
      4, 0, 0, 0,
      'a',
      'b',
      0,
      '$',
   };

   char data[] = { 'a', 'b', 0, '$' };

   testCollection<CharVector>(data, expected);
}

void TestSerialization::testUIntCollections()
{
   const char expected[] = {
      16, 0, 0, 0,
      2, 0, 0, 0,
      1, 2, 3, 4,
      2, 3, 4, 5
   };

   uint32_t data[] = { 0x04030201, 0x05040302 };

   testCollection<UIntList>(data, expected);
}

void TestSerialization::testIntCollections()
{
   const char expected[] = {
      16, 0, 0, 0,
      2, 0, 0, 0,
      1, 2, 3, 4,
      2, 3, 4, 5
   };

   int32_t data[] = { 0x04030201, 0x05040302 };

   testCollection<IntList>(data, expected);
}

void TestSerialization::testStringCollections()
{
   const char expected[] = {
      15, 0, 0, 0,
      3, 0, 0, 0,
      0,
      'a', 's', 0,
      'd', 'f', 0,
   };

   std::string data[] = { "", "as", "df" };

   testCollection<StringSet>(data, expected);
   testCollection<StringList>(data, expected);
}

void TestSerialization::testHighRestStatCollections()
{
   const char expected[] = {
      2, 0, 0, 0,
      1, 0, 0, 0, 0, 0, 0, 0,
         2, 0, 0, 0,
         3, 0, 0, 0,
         4, 0, 0, 0, 0, 0, 0, 0,
         5, 0, 0, 0, 0, 0, 0, 0,
         6, 0, 0, 0, 0, 0, 0, 0,
         7, 0, 0, 0, 0, 0, 0, 0,
         8, 0, 0, 0,
      2, 0, 0, 0, 0, 0, 0, 0,
         3, 0, 0, 0,
         4, 0, 0, 0,
         5, 0, 0, 0, 0, 0, 0, 0,
         6, 0, 0, 0, 0, 0, 0, 0,
         7, 0, 0, 0, 0, 0, 0, 0,
         8, 0, 0, 0, 0, 0, 0, 0,
         9, 0, 0, 0,
   };

   HighResolutionStats data[] = {
      { { 1, 2, 3 }, { 4, 5, 6, 7, 8 } },
      { { 2, 3, 4 }, { 5, 6, 7, 8, 9 } },
   };

   struct op
   {
      static bool eq(const HighResolutionStats& a, const HighResolutionStats& b)
      {
         return a.rawVals.statsTimeMS == b.rawVals.statsTimeMS
            && a.rawVals.busyWorkers == b.rawVals.busyWorkers
            && a.rawVals.queuedRequests == b.rawVals.queuedRequests
            && a.incVals.diskWriteBytes == b.incVals.diskWriteBytes
            && a.incVals.diskReadBytes == b.incVals.diskReadBytes
            && a.incVals.netSendBytes == b.incVals.netSendBytes
            && a.incVals.netRecvBytes == b.incVals.netRecvBytes
            && a.incVals.workRequests == b.incVals.workRequests;
      }
   };

   testCollection<HighResStatsList>(data, expected, op::eq);
}

void TestSerialization::testNicAddressList()
{
   const char expected[] = {
      2, 0, 0, 0,
      127, 0, 0, 1,
      's', 'n', 'a', 'f', 'u', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0,
      0, 0, 0,
      10, 42, 23, 17,
      't', 'e', 's', 't', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 0,
      2,
      0, 0, 0,
   };

   NicAddress data[] = {
      { {INADDR_ANY}, {INADDR_ANY}, 0, 0, NICADDRTYPE_STANDARD, "snafu", {} },
      { {INADDR_ANY}, {INADDR_ANY}, 0, 0, NICADDRTYPE_RDMA, "test123456789ab", {} },
   };

   data[0].ipAddr.s_addr = htonl(0x7f000001);
   data[1].ipAddr.s_addr = htonl(0x0a2a1711);

   struct op
   {
      static bool eq(const NicAddress& a, const NicAddress& b)
      {
         return a.ipAddr.s_addr == b.ipAddr.s_addr
            && ::strncmp(a.name, b.name, IFNAMSIZ) == 0
            && a.nicType == b.nicType;
      }
   };

   testCollection<NicAddressList>(data, expected, data::NicAddress::eq);
}

void TestSerialization::testNodeList()
{
   std::cerr << "\ntest " << __PRETTY_FUNCTION__ << " is disabled due to tight coupling";
   return;

   const char expected[] = {
      1, 0, 0, 0, 0, 0, 0, 0,
      32, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
      4, 0, 0, 0, 'a', 'b', 'c', 'd', 0, 0, 0, 0,
      1, 0, 0, 0, 127, 0, 0, 1, 's', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,
      1, 0, 0, 0,
      2, 0, 0, 0,
      1, 2,
      3, 4,
      4,
      0, 0, 0,
   };

   NicAddress addr = { {INADDR_ANY}, {INADDR_ANY}, 0, 0, NICADDRTYPE_RDMA, {'s'}, {} };
   addr.ipAddr.s_addr = htonl(0x7f000001);

   BitStore features(32);
   features.setBit(0, true);

   NicAddressList addrs(1, addr);

   auto node = std::make_shared<Node>("abcd", NumNodeID(2), 0x0201, 0x0403, addrs);
   node->setFeatureFlags(&features);
   node->setFhgfsVersion(1);
   node->setNodeType(NODETYPE_Mgmt);

   struct ops
   {
      static bool eq(const NodeHandle& a, const NodeHandle& b)
      {
         return *a->getNodeFeatures() == *b->getNodeFeatures()
            && a->getID() == b->getID()
            && data::NicAddress::listEq(a->getNicList(), b->getNicList() )
            && a->getFhgfsVersion() == b->getFhgfsVersion()
            && a->getNumID() == b->getNumID()
            && a->getPortUDP() == b->getPortUDP()
            && a->getPortTCP() == b->getPortTCP()
            && a->getNodeType() == b->getNodeType();
      }
   };

   NodeHandle nodes[] = { node };

   return testCollection<std::vector<NodeHandle>>(nodes, expected, ops::eq);
}

void TestSerialization::testBuddyMirrorPattern()
{
   const char expected[] = {
      // header
      // -> pattern length
      28, 0, 0, 0,
      // -> pattern type
      StripePatternType_BuddyMirror, 0, 0, 0,
      // -> chunk size
      '\xfe', '\xff', 0, 0,
      // pattern data
      // -> defaultNumTargets
      4, 0, 0, 0,
      // -> group ids
      12, 0, 0, 0,
      2, 0, 0, 0,
      4, 1,
      2, 3,
   };

   UInt16Vector groups;
   groups.push_back(0x0104);
   groups.push_back(0x0302);

   BuddyMirrorPattern pattern(0xfffe, groups, 4);
   testStripePattern(pattern, expected);
}

void TestSerialization::testRaid0Pattern()
{
   Raid0Pattern value = data::Raid0Pattern::value;
   testStripePattern(value, data::Raid0Pattern::expected);
}

void TestSerialization::testRaid10Pattern()
{
   const char expected[] = {
      // header
      // -> pattern length
      40, 0, 0, 0,
      // -> pattern type
      StripePatternType_Raid10, 0, 0, 0,
      // -> chunk size
      '\xfe', '\xff', 0, 0,
      // pattern data
      // -> defaultNumTargets
      4, 0, 0, 0,
      // -> stripe target ids
      12, 0, 0, 0,
      2, 0, 0, 0,
      4, 1,
      2, 3,
      // -> mirror target ids
      12, 0, 0, 0,
      2, 0, 0, 0,
      5, 2,
      3, 4,
   };

   UInt16Vector stripes;
   stripes.push_back(0x0104);
   stripes.push_back(0x0302);

   UInt16Vector mirrors;
   mirrors.push_back(0x0205);
   mirrors.push_back(0x0403);

   Raid10Pattern pattern(0xfffe, stripes, mirrors, 4);
   testStripePattern(pattern, expected);
}

void TestSerialization::testFsckDirInode()
{
   const char expected[] = {
      // id
      5, 0, 0, 0,
      '1', '-', '2', '-', '3', 0,
      0, 0,
      // parent id
      5, 0, 0, 0,
      '2', '-', '3', '-', '4', 0,
      0, 0,
      // parent node id
      1, 2, 3, 4,
      // owner node id
      2, 3, 4, 5,
      // size
      1, 2, 3, 4, 5, 6, 7, 8,
      // num hardlinks
      2, 3, 0, 0,
      // stripe targets
      12, 0, 0, 0,
      2, 0, 0, 0,
      1, 0,
      1, 1,
      // stripe pattern type
      FsckStripePatternType_RAID0, 0, 0, 0,
      // save node id
      9, 8, 0, 1,
      // mirrored
      1,
      // readable
      0,
      // mismirrored
      0,
   };

   UInt16Vector stripeTargets;
   stripeTargets.push_back(1);
   stripeTargets.push_back(0x0101);

   FsckDirInode inode("1-2-3", "2-3-4", NumNodeID(0x04030201), NumNodeID(0x05040302),
      0x0807060504030201LL, 0x0302, stripeTargets, FsckStripePatternType_RAID0,
      NumNodeID(0x01000809), true, false, false);

   testObject(inode, expected);
}

void TestSerialization::testFsckFileInode()
{
   const char expected[] = {
      // id
      5, 0, 0, 0, '1', '-', '2', '-', '3', 0, 0, 0,
      // parent id
      5, 0, 0, 0, '2', '-', '3', '-', '4', 0, 0, 0,
      // parent node id
      1, 2, 3, 4,
      // path info
      PATHINFO_FEATURE_ORIG, 0, 0, 0,
      42, 0, 0, 0,
      5, 0, 0, 0, '4', '-', '3', '-', '2', 0, 0, 0,
      // uid
      1, 0, 0, 0,
      // gid
      2, 0, 0, 0,
      // fileSize
      0, 1, 0, 0, 0, 0, 0, 0,
      // hardlinks
      4, 0, 0, 0,
      // usedBlocks
      5, 0, 0, 0, 0, 0, 0, 0,
      // stripe targets
      12, 0, 0, 0,
      2, 0, 0, 0,
      1, 0,
      1, 1,
      // stripe pattern type
      FsckStripePatternType_RAID0, 0, 0, 0,
      // chunk size
      '\xff', '\xff', 0, 0,
      // save node id
      9, 8, 0, 1,
      // save inode
      8, 7, 6, 5, 4, 3, 2, 1,
      // save device
      1, 2, 3, 4,
      // inlined
      1,
      // mirrored
      0,
      // readable
      1,
      // mismirrored
      0,
   };

   PathInfo pathInfo(42, "4-3-2", PATHINFO_FEATURE_ORIG);

   UInt16Vector stripeTargets;
   stripeTargets.push_back(1);
   stripeTargets.push_back(0x0101);

   FsckFileInode inode("1-2-3", "2-3-4", NumNodeID(0x04030201), pathInfo, 1, 2, 256, 4, 5,
         stripeTargets, FsckStripePatternType_RAID0, 0xffff, NumNodeID(0x01000809),
         0x0102030405060708ULL, 0x04030201, true, false, true, false);

   testObject(inode, expected);
}

void TestSerialization::testEntryInfo()
{
   EntryInfo info = data::EntryInfo::value;
   const char (&expected)[54] = data::EntryInfo::expected;

   testObject(info, expected, NoModifier(), data::EntryInfo::eq);
}

void TestSerialization::testEntryInfoWithDepth()
{
   EntryInfoWithDepth info = data::EntryInfoWithDepth::value;
   const char (&expected)[58] = data::EntryInfoWithDepth::expected;

   testObject(info, expected, NoModifier(), data::EntryInfoWithDepth::eq);
}

void TestSerialization::testMkLocalDirMsg()
{
   const char expected[] = {
      // uid
      1, 0, 0, 0,
      // gid
      2, 0, 0, 0,
      // mode
      3, 0, 0, 0,
      // entryInfo
      TEST_SER_ENTRYINFO_RAW,
      // stripePattern
      TEST_SER_RAID0PATTERN_RAW,
      // parentNodeID
      4, 0, 0, 0,
      // defaultACLXAttr
      2, 0, 0, 0, 1, 2,
      // accessACLXAttr
      2, 0, 0, 0, 3, 4,
   };

   EntryInfo ei = data::EntryInfo::value;
   Raid0Pattern pattern = data::Raid0Pattern::value;

   CharVector defaultACL;
   defaultACL.push_back(1);
   defaultACL.push_back(2);

   CharVector accessACL;
   accessACL.push_back(3);
   accessACL.push_back(4);

   MkLocalDirMsg msg(&ei, 1, 2, 3, &pattern, NumNodeID(4), defaultACL, accessACL);

   struct op
   {
      static bool eq(MkLocalDirMsg& a, MkLocalDirMsg& b)
      {
         return a.getUserID() == b.getUserID()
            && a.getGroupID() == b.getGroupID()
            && a.getMode() == b.getMode()
            && data::EntryInfo::eq(*a.getEntryInfo(), *b.getEntryInfo() )
            && a.getParentNodeID() == b.getParentNodeID()
            && a.getDefaultACLXAttr() == b.getDefaultACLXAttr()
            && a.getAccessACLXAttr() == b.getAccessACLXAttr();
      }
   };

   testMessage(msg, expected, op::eq);
}

void TestSerialization::testSetXAttrMsg()
{
   const char input[] = {
      // entryInfo
      TEST_SER_ENTRYINFO_RAW,
      // name
      5, 0, 0, 0,
      'x', 'a', 't', 't', 'r', 0,
      // value
      2, 0, 0, 0, 1, 2,
      // flags
      42, 0, 0, 0,
   };

   SetXAttrMsg msg;

   CPPUNIT_ASSERT(msg.deserializePayload(input, sizeof(input) ) );

   CPPUNIT_ASSERT(data::EntryInfo::eq(*msg.getEntryInfo(), data::EntryInfo::value) );
   CPPUNIT_ASSERT(msg.getName() == "xattr");

   CPPUNIT_ASSERT(msg.getValue().size() == 2);
   CPPUNIT_ASSERT(msg.getValue()[0] == 1);
   CPPUNIT_ASSERT(msg.getValue()[1] == 2);

   CPPUNIT_ASSERT(msg.getFlags() == 42);

   CPPUNIT_ASSERT(!msg.deserializePayload(input, sizeof(input) - 1) );
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

void TestSerialization::testSerializeAs()
{
   {
      const char expected[] = {
         1, 0,
         '\xfe', '\xff',
      };

      testPrimitiveType(std::make_tuple(TestEnum1::first, TestEnum1::second), expected);
   }

   {
      const char expected[] = { 1, 0, 0, 0, 0, 0, 0, 0, };

      TestEnum1 value = TestEnum1::first;
      testObject(value, expected, EnumAsMod(), std::equal_to<TestEnum1>());
   }
}
