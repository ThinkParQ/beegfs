#ifndef source_ReadWrite_h_aRnD7hojuRYcvdXUFEvHkf
#define source_ReadWrite_h_aRnD7hojuRYcvdXUFEvHkf

#include <cppunit/extensions/HelperMacros.h>
#include "SafeFD.h"
#include "FileDescription.h"
#include "FileUnlinker.h"

enum tfd_flags {
   TFD_DEFAULT = 0,

   TFD_SPAN = 1,
   TFD_NO_UNLINK = 2,

   TFD_SYNC_BY_FSYNC = 4,
   TFD_SYNC_BY_CLOSE = 0,
   TFD_SYNC_NEVER = 8,
};

inline tfd_flags operator|(tfd_flags a, tfd_flags b)
{
   return tfd_flags(int(a) | int(b));
}

class ReadWriteTest : public CPPUNIT_NS::TestFixture {
   CPPUNIT_TEST_SUITE(ReadWriteTest);

   CPPUNIT_TEST(singleByte);

   CPPUNIT_TEST(smallFileLocal);
   CPPUNIT_TEST(smallFileSpan);
   CPPUNIT_TEST(smallFileUnflushed);
   CPPUNIT_TEST(bigFileLocal);
   CPPUNIT_TEST(bigFileSpan);
   CPPUNIT_TEST(bigFileSpan_NoUnlink);

   CPPUNIT_TEST(seekLocal);
   CPPUNIT_TEST(seekSpanWithSync);
   CPPUNIT_TEST(seekWriteSyncReread);

   CPPUNIT_TEST(readIOError);
   CPPUNIT_TEST(writeIOError);

   CPPUNIT_TEST(writeSyncWriteMore);

   CPPUNIT_TEST(concurrentPageModify);
   CPPUNIT_TEST(concurrentAppend);

   CPPUNIT_TEST(readWriteSpanDirect);

   CPPUNIT_TEST(manySmallWrites);

   CPPUNIT_TEST(largeSparseDirect);

   CPPUNIT_TEST(mapModifyMap);

   CPPUNIT_TEST(truncate);
   CPPUNIT_TEST(appendTruncateAppend);
   CPPUNIT_TEST(appendWriteAppend);

   CPPUNIT_TEST(noIsizeDecrease);

   CPPUNIT_TEST(writeCacheBypass);
   CPPUNIT_TEST(readCacheBypass);

   CPPUNIT_TEST_SUITE_END();

   protected:
      void doSmallFile(tfd_flags flags);
      void doBigFile(tfd_flags flags);

      void singleByte();
      void fsyncCommunicate();
      void seekWriteSyncReread();

      void doSeek(tfd_flags flags);

      void readIOError();
      void writeIOError();

      void writeSyncWriteMore();

      void concurrentPageModify();
      void concurrentAppend();

      void readWriteSpanDirect();

      void manySmallWrites();

      void largeSparseDirect();

      void mapModifyMap();

      void truncate();
      void appendTruncateAppend();
      void appendWriteAppend();

      void noIsizeDecrease();

      void writeCacheBypass();
      void readCacheBypass();

      void testFileDescription(FileDescription& file, tfd_flags flags);

   protected:
      void smallFileLocal() { doSmallFile(TFD_DEFAULT); }
      void smallFileSpan() { doSmallFile(TFD_SPAN); }
      void smallFileUnflushed() { doSmallFile(TFD_SYNC_NEVER); }
      void bigFileLocal() { doBigFile(TFD_DEFAULT); }
      void bigFileSpan() { doBigFile(TFD_SPAN); }
      void bigFileSpan_NoUnlink()
      {
         doBigFile(TFD_SPAN | TFD_NO_UNLINK);
         doBigFile(TFD_SPAN);
      }
      void seekLocal() { doSeek(TFD_DEFAULT); }
      void seekSpanWithSync() { doSeek(TFD_SPAN | TFD_SYNC_BY_FSYNC); }
};

#endif
