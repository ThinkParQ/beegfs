#include "TestSerialization.h"

#include <common/storage/EntryInfo.h>
#include <common/storage/striping/BuddyMirrorPattern.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/storage/striping/Raid10Pattern.h>
#include <common/toolkit/ZipIterator.h>
#include <session/Session.h>

TEST_F(TestSerialization, sessionSerialization)
{
   SessionStore sessionStore;
   SessionStore sessionStoreClone;

   initSessionStoreForTests(sessionStore);

   size_t bufLen;
   {
      Serializer ser;
      sessionStore.serialize(ser);
      bufLen = ser.size();
   }

   boost::scoped_array<char> buf(new char[bufLen]);

   Serializer ser(buf.get(), bufLen);
   sessionStore.serialize(ser);
   if (!ser.good() || ser.size() != bufLen)
      FAIL() << "Serialization failed!";

   Deserializer des(buf.get(), bufLen);
   sessionStoreClone.deserialize(des);
   if (!des.good())
      FAIL() << "Deserialization failed!";

   if(sessionStore != sessionStoreClone)
      FAIL() << "Sessions is not equal after serialization and deserialization!";
}

void TestSerialization::initSessionStoreForTests(SessionStore& testSessionStore)
{
   SettableFileAttribs* settableFileAttribs10 = new SettableFileAttribs();
   settableFileAttribs10->groupID = UINT_MAX;
   settableFileAttribs10->lastAccessTimeSecs = std::numeric_limits<int64_t>::max();
   settableFileAttribs10->mode = INT_MAX;
   settableFileAttribs10->modificationTimeSecs = std::numeric_limits<int64_t>::max();
   settableFileAttribs10->userID = UINT_MAX;
   size_t numTargets10 = 25;
   StatData* statData10 = new StatData(std::numeric_limits<int64_t>::min(), settableFileAttribs10,
      std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::min(), 0, 0);
   statData10->setSparseFlag();
   UInt16Vector* targets10 = new UInt16Vector(numTargets10);
   TestSerialization::addRandomValuesToUInt16Vector(targets10, numTargets10);
   UInt16Vector* mirrorTargets10 = new UInt16Vector(numTargets10);
   TestSerialization::addRandomValuesToUInt16Vector(mirrorTargets10, numTargets10);
   TestSerialization::addRandomTargetChunkBlocksToStatData(statData10, numTargets10);
   Raid10Pattern* stripePattern10 = new Raid10Pattern(UINT_MAX, *targets10, *mirrorTargets10,
      numTargets10);
   std::string* entryID10 = new std::string("0-00ADF231-7");
   std::string* origParentEntryID10 = new std::string("2-AFD734AF-6");
   FileInodeStoreData* fileInodeStoreData10 = new FileInodeStoreData(*entryID10, statData10,
      stripePattern10, FILEINODE_FEATURE_HAS_ORIG_UID, 1345, *origParentEntryID10,
      FileInodeOrigFeature_TRUE);
   FileInode* inode10 = new FileInode(*entryID10, fileInodeStoreData10, DirEntryType_REGULARFILE,
      UINT_MAX);
   inode10->setIsInlinedUnlocked(false);
   inode10->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo10 = new EntryInfo(NumNodeID(345), *origParentEntryID10, *entryID10,
      "testfile10", DirEntryType_REGULARFILE, 0);

   SettableFileAttribs* settableFileAttribs11 = new SettableFileAttribs();
   settableFileAttribs11->groupID = 0;
   settableFileAttribs11->lastAccessTimeSecs = std::numeric_limits<int64_t>::min();
   settableFileAttribs11->mode = INT_MIN;
   settableFileAttribs11->modificationTimeSecs = std::numeric_limits<int64_t>::min();
   settableFileAttribs11->userID = 0;
   size_t numTargets11 = 2487;
   StatData* statData11 = new StatData(std::numeric_limits<int64_t>::max(), settableFileAttribs11,
      std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::max(),
      UINT_MAX, UINT_MAX);
   statData11->setSparseFlag();
   UInt16Vector* targets11 = new UInt16Vector(numTargets11);
   TestSerialization::addRandomValuesToUInt16Vector(targets11, numTargets11);
   TestSerialization::addRandomTargetChunkBlocksToStatData(statData11, numTargets11);
   Raid0Pattern* stripePattern11 = new Raid0Pattern(1024, *targets11, numTargets11);
   std::string* entryID11 = new std::string("1-345234AF-4");
   std::string* origParentEntryID11 = new std::string("1-892CD123-9");
   FileInodeStoreData* fileInodeStoreData11 = new FileInodeStoreData(*entryID11, statData11,
      stripePattern11, FILEINODE_FEATURE_HAS_ORIG_UID, 145, *origParentEntryID11,
      FileInodeOrigFeature_TRUE);
   FileInode* inode11 = new FileInode(*entryID11, fileInodeStoreData11, DirEntryType_REGULARFILE,
      0);
   inode11->setIsInlinedUnlocked(true);
   inode11->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo11 = new EntryInfo(NumNodeID(45), *origParentEntryID11, *entryID11,
      "testfile11", DirEntryType_FIFO, ENTRYINFO_FEATURE_INLINED);

   SettableFileAttribs* settableFileAttribs12 = new SettableFileAttribs();
   settableFileAttribs12->groupID = 457;
   settableFileAttribs12->lastAccessTimeSecs = 7985603;
   settableFileAttribs12->mode = 785;
   settableFileAttribs12->modificationTimeSecs = 54113;
   settableFileAttribs12->userID = 784;
   size_t numTargets12 = 22;
   StatData* statData12 = new StatData(55568, settableFileAttribs12, 785133, 92334, 45, 78);
   statData12->setSparseFlag();
   UInt16Vector* targets12 = new UInt16Vector(numTargets12);
   TestSerialization::addRandomValuesToUInt16Vector(targets12, numTargets12);
   BuddyMirrorPattern* stripePattern12 = new BuddyMirrorPattern(4096, *targets12, numTargets12);
   std::string* entryID12 = new std::string("8-DC12AF12-3");
   std::string* origParentEntryID12 = new std::string("9-FE998021-3");
   FileInodeStoreData* fileInodeStoreData12 = new FileInodeStoreData(*entryID12, statData12,
      stripePattern12, FILEINODE_FEATURE_HAS_ORIG_UID, 1789, *origParentEntryID12,
      FileInodeOrigFeature_TRUE);
   FileInode* inode12 = new FileInode(*entryID12, fileInodeStoreData12, DirEntryType_REGULARFILE,
      DENTRY_FEATURE_IS_FILEINODE);
   inode12->setIsInlinedUnlocked(true);
   inode12->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo12 = new EntryInfo(NumNodeID(789), *origParentEntryID12, *entryID12,
      "testfile12", DirEntryType_DIRECTORY, 0);

   SettableFileAttribs* settableFileAttribs13 = new SettableFileAttribs();
   settableFileAttribs13->groupID = 556;
   settableFileAttribs13->lastAccessTimeSecs =798336;
   settableFileAttribs13->mode = 456;
   settableFileAttribs13->modificationTimeSecs = 12383635;
   settableFileAttribs13->userID = 886;
   size_t numTargets13 = 55;
   StatData* statData13 = new StatData(775616, settableFileAttribs13, 56633, 565663, 7893, 1315);
   UInt16Vector* targets13 = new UInt16Vector(numTargets13);
   TestSerialization::addRandomValuesToUInt16Vector(targets13, numTargets13);
   BuddyMirrorPattern* stripePattern13 = new BuddyMirrorPattern(10240, *targets13, numTargets13);
   std::string* entryID13 = new std::string("3-CDF1239F-9");
   std::string* origParentEntryID13 = new std::string("4-DFA23A31-2");
   FileInodeStoreData* fileInodeStoreData13 = new FileInodeStoreData(*entryID13, statData13,
      stripePattern13, FILEINODE_FEATURE_HAS_ORIG_UID, 1892, *origParentEntryID13,
      FileInodeOrigFeature_TRUE);
   FileInode* inode13 = new FileInode(*entryID13, fileInodeStoreData13, DirEntryType_REGULARFILE,
      DENTRY_FEATURE_IS_FILEINODE);
   inode13->setIsInlinedUnlocked(false);
   inode13->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo13 = new EntryInfo(NumNodeID(892), *origParentEntryID13, *entryID13,
      "testfile13", DirEntryType_REGULARFILE, ENTRYINFO_FEATURE_INLINED);

   SettableFileAttribs* settableFileAttribs14 = new SettableFileAttribs();
   settableFileAttribs14->groupID = 8963;
   settableFileAttribs14->lastAccessTimeSecs = 3468869;
   settableFileAttribs14->mode = 6858;
   settableFileAttribs14->modificationTimeSecs = 167859;
   settableFileAttribs14->userID = 23;
   size_t numTargets14 = 2;
   StatData* statData14 = new StatData(5435585, settableFileAttribs14, 12855, 78, 965, 55);
   statData14->setSparseFlag();
   UInt16Vector* targets14 = new UInt16Vector(numTargets14);
   TestSerialization::addRandomValuesToUInt16Vector(targets14, numTargets14);
   TestSerialization::addRandomTargetChunkBlocksToStatData(statData14, numTargets14);
   Raid0Pattern* stripePattern14 = new Raid0Pattern (512, *targets14, numTargets14);
   std::string* entryID14 = new std::string("4-345234AF-5");
   std::string* origParentEntryID14 = new std::string("8-AB782DDF-6");
   FileInodeStoreData* fileInodeStoreData14 = new FileInodeStoreData(*entryID14, statData14,
      stripePattern14, FILEINODE_FEATURE_HAS_ORIG_UID, 11021, *origParentEntryID14,
      FileInodeOrigFeature_TRUE);
   FileInode* inode14 = new FileInode(*entryID14, fileInodeStoreData14, DirEntryType_REGULARFILE,
      DENTRY_FEATURE_IS_FILEINODE);
   inode14->setIsInlinedUnlocked(false);
   inode14->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo14 = new EntryInfo(NumNodeID(1021), *origParentEntryID14, *entryID14,
      "testfile14", DirEntryType_SYMLINK, ENTRYINFO_FEATURE_INLINED);


   SettableFileAttribs* settableFileAttribs20 = new SettableFileAttribs();
   settableFileAttribs20->groupID = 6644;
   settableFileAttribs20->lastAccessTimeSecs = 546463;
   settableFileAttribs20->mode = 882;
   settableFileAttribs20->modificationTimeSecs = 45446;
   settableFileAttribs20->userID = 556;
   size_t numTargets20 = 3;
   StatData* statData20 = new StatData(95483, settableFileAttribs20, 35465, 4556, 78, 62);
   statData20->setSparseFlag();
   UInt16Vector* targets20 = new UInt16Vector(numTargets20);
   TestSerialization::addRandomValuesToUInt16Vector(targets20, 1);
   targets20->at(1) = std::numeric_limits<uint16_t>::max();
   targets20->at(2) = 0;
   TestSerialization::addRandomTargetChunkBlocksToStatData(statData20, numTargets20);
   Raid0Pattern* stripePattern20 = new Raid0Pattern(2048, *targets20, numTargets20);
   std::string* entryID20 = new std::string("9-AF891DC1-1");
   std::string* origParentEntryID20 = new std::string("4-DFA4512F-7");
   FileInodeStoreData* fileInodeStoreData20 = new FileInodeStoreData(*entryID20, statData20,
      stripePattern20, 0, 2331, *origParentEntryID20, FileInodeOrigFeature_TRUE);
   FileInode* inode20 = new FileInode(*entryID20, fileInodeStoreData20, DirEntryType_REGULARFILE,
      DENTRY_FEATURE_IS_FILEINODE);
   inode20->setIsInlinedUnlocked(true);
   inode20->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo20 = new EntryInfo(NumNodeID(331), *origParentEntryID20, *entryID20,
      "testfile20", DirEntryType_REGULARFILE, INT_MIN);

   SettableFileAttribs* settableFileAttribs21 = new SettableFileAttribs();
   settableFileAttribs21->groupID = 45;
   settableFileAttribs21->lastAccessTimeSecs = 5416131;
   settableFileAttribs21->mode = 533;
   settableFileAttribs21->modificationTimeSecs = 5441363;
   settableFileAttribs21->userID = 5823;
   size_t numTargets21 = 6;
   StatData* statData21 = new StatData(854721, settableFileAttribs21, 62685, 756416, 7864, 225);
   statData21->setSparseFlag();
   UInt16Vector* targets21 = new UInt16Vector(numTargets21);
   TestSerialization::addRandomValuesToUInt16Vector(targets21, numTargets21);
   UInt16Vector* mirrorTargets21 = new UInt16Vector(numTargets21);
   TestSerialization::addRandomValuesToUInt16Vector(mirrorTargets21, numTargets21);
   TestSerialization::addRandomTargetChunkBlocksToStatData(statData21, numTargets21);
   Raid10Pattern* stripePattern21 = new Raid10Pattern (512, *targets21, *mirrorTargets21,
      numTargets21);
   std::string* entryID21 = new std::string("0-34A23FAB-6");
   std::string* origParentEntryID21 = new std::string("2-AFAF3444-6");
   FileInodeStoreData* fileInodeStoreData21 = new FileInodeStoreData(*entryID21, statData21,
      stripePattern21, UINT_MAX, 2890, *origParentEntryID21, FileInodeOrigFeature_TRUE);
   FileInode* inode21 = new FileInode(*entryID21, fileInodeStoreData21, DirEntryType_REGULARFILE,
      DENTRY_FEATURE_IS_FILEINODE);
   inode21->setIsInlinedUnlocked(false);
   inode21->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo21 = new EntryInfo(NumNodeID(890), *origParentEntryID21, *entryID21,
      "testfile21", DirEntryType_DIRECTORY, ENTRYINFO_FEATURE_INLINED);

   SettableFileAttribs* settableFileAttribs22 = new SettableFileAttribs();
   settableFileAttribs22->groupID = 7889;
   settableFileAttribs22->lastAccessTimeSecs = 1316313;
   settableFileAttribs22->mode = 5263;
   settableFileAttribs22->modificationTimeSecs = 1313416;
   settableFileAttribs22->userID = 5555;
   size_t numTargets22 = 42;
   StatData* statData22 = new StatData(9613463, settableFileAttribs22, 7646416, 6116546, 788, 4565);
   statData22->setSparseFlag();
   UInt16Vector* targets22 = new UInt16Vector(numTargets22);
   TestSerialization::addRandomValuesToUInt16Vector(targets22, numTargets22);
   TestSerialization::addRandomTargetChunkBlocksToStatData(statData22, numTargets22);
   BuddyMirrorPattern* stripePattern22 = new BuddyMirrorPattern (1024, *targets22, numTargets22);
   std::string* entryID22 = new std::string("1-34F23DF1-5");
   std::string* origParentEntryID22 = new std::string("8-734AF123-1");
   FileInodeStoreData* fileInodeStoreData22 = new FileInodeStoreData(*entryID22, statData22,
      stripePattern22, FILEINODE_FEATURE_HAS_ORIG_UID, UINT_MAX, *origParentEntryID22,
      FileInodeOrigFeature_TRUE);
   FileInode* inode22 = new FileInode(*entryID22, fileInodeStoreData22, DirEntryType_REGULARFILE,
      DENTRY_FEATURE_IS_FILEINODE);
   inode22->setIsInlinedUnlocked(true);
   inode22->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo22 = new EntryInfo(NumNodeID(453), *origParentEntryID22, *entryID22,
      "testfile22", DirEntryType_SYMLINK, ENTRYINFO_FEATURE_INLINED);


   SettableFileAttribs* settableFileAttribs30 = new SettableFileAttribs();
   settableFileAttribs30->groupID = 5646;
   settableFileAttribs30->lastAccessTimeSecs = 11566300;
   settableFileAttribs30->mode = 556;
   settableFileAttribs30->modificationTimeSecs = 564633;
   settableFileAttribs30->userID = 44;
   size_t numTargets30 = 111;
   StatData* statData30 = new StatData(73663, settableFileAttribs30, 555, 874496, 346, 798);
   statData30->setSparseFlag();
   UInt16Vector* targets30 = new UInt16Vector(numTargets30);
   TestSerialization::addRandomValuesToUInt16Vector(targets30, numTargets30);
   TestSerialization::addRandomTargetChunkBlocksToStatData(statData30, numTargets30);
   Raid0Pattern* stripePattern30 = new Raid0Pattern (4096, *targets30, numTargets30);
   std::string* entryID30 = new std::string("2-893ADF21-9");
   std::string* origParentEntryID30 = new std::string("5-DA474213-8");
   FileInodeStoreData* fileInodeStoreData30 = new FileInodeStoreData(*entryID30, statData30,
      stripePattern30, FILEINODE_FEATURE_HAS_ORIG_UID, 3121, *origParentEntryID30,
      FileInodeOrigFeature_TRUE);
   FileInode* inode30 = new FileInode(*entryID30, fileInodeStoreData30, DirEntryType_REGULARFILE,
      DENTRY_FEATURE_IS_FILEINODE);
   inode30->setIsInlinedUnlocked(true);
   inode30->initLocksRandomForSerializationTests();
   EntryInfo* entryInfo30 = new EntryInfo(NumNodeID(std::numeric_limits<uint16_t>::max() ),
      *origParentEntryID30, *entryID30, "testfile30", DirEntryType_REGULARFILE, INT_MAX);



   SessionFile* sessionFile10 = new SessionFile({inode10, 0}, OPENFILE_ACCESS_WRITE |
      OPENFILE_ACCESS_SYNC, entryInfo10);
   sessionFile10->setSessionID(2344);
   sessionFile10->setUseAsyncCleanup();

   SessionFile* sessionFile11 = new SessionFile({inode11, 0}, OPENFILE_ACCESS_READWRITE,
      entryInfo11);
   sessionFile11->setSessionID(465);

   SessionFile* sessionFile12 = new SessionFile({inode12, 0}, OPENFILE_ACCESS_READ |
      OPENFILE_ACCESS_TRUNC, entryInfo12);
   sessionFile12->setSessionID(3599);

   SessionFile* sessionFile13 = new SessionFile({inode13, 0}, OPENFILE_ACCESS_WRITE |
      OPENFILE_ACCESS_APPEND, entryInfo13);
   sessionFile13->setSessionID(8836);
   sessionFile13->setUseAsyncCleanup();

   SessionFile* sessionFile14 = new SessionFile({inode14, 0}, OPENFILE_ACCESS_WRITE |
      OPENFILE_ACCESS_DIRECT, entryInfo14);
   sessionFile14->setSessionID(110);


   SessionFile* sessionFile20 = new SessionFile({inode20, 0}, OPENFILE_ACCESS_WRITE |
      OPENFILE_ACCESS_APPEND, entryInfo20);
   sessionFile20->setSessionID(0);

   SessionFile* sessionFile21 = new SessionFile({inode21, 0}, OPENFILE_ACCESS_READ, entryInfo21);
   sessionFile21->setSessionID(11);
   sessionFile21->setUseAsyncCleanup();

   SessionFile* sessionFile22 = new SessionFile({inode22, 0}, OPENFILE_ACCESS_READWRITE,
      entryInfo22);
   sessionFile22->setSessionID(7846);

   SessionFile* sessionFile30 = new SessionFile({inode30, 0}, OPENFILE_ACCESS_WRITE |
      OPENFILE_ACCESS_SYNC, entryInfo30);
   sessionFile30->setSessionID(UINT_MAX);



   Session* testSession1 = new Session(NumNodeID(1) );
   testSession1->getFiles()->addSession(sessionFile10);
   testSession1->getFiles()->addSession(sessionFile11);
   testSession1->getFiles()->addSession(sessionFile12);
   testSession1->getFiles()->addSession(sessionFile13);
   testSession1->getFiles()->addSession(sessionFile14);

   Session* testSession2 = new Session(NumNodeID(100) );
   testSession2->getFiles()->addSession(sessionFile20);
   testSession2->getFiles()->addSession(sessionFile21);
   testSession2->getFiles()->addSession(sessionFile22);

   Session* testSession3 = new Session(NumNodeID(100) );
   testSession3->getFiles()->addSession(sessionFile30);


   testSessionStore.addSession(testSession1);
   testSessionStore.addSession(testSession2);
   testSessionStore.addSession(testSession3);
}

TEST_F(TestSerialization, dynamicFileAttribs)
{
   DynamicFileAttribs attribs(1, 2, 3, 4, 5);

   testObjectRoundTrip(attribs);
}

TEST_F(TestSerialization, chunkFileInfo)
{
   DynamicFileAttribs attribs(1, 2, 3, 4, 5);
   ChunkFileInfo info(1, 0, attribs);

   testObjectRoundTrip(info);
}

TEST_F(TestSerialization, entryLockDetails)
{
   EntryLockDetails eld(NumNodeID(1), 2, 3, "12345", 6);

   testObjectRoundTrip(eld);
}

TEST_F(TestSerialization, rangeLockDetails)
{
   RangeLockDetails rld(NumNodeID(1), 2, "12345", 6, 7, 8);

   testObjectRoundTrip(rld);
}

void TestSerialization::addRandomValuesToUInt16Vector(UInt16Vector* vector, size_t count)
{
   Random rand;

   for(size_t i = 0; i < count; i++)
   {
      uint16_t value = rand.getNextInRange(0, std::numeric_limits<uint16_t>::max() );
      vector->at(i) = value;
   }
}

void TestSerialization::addRandomTargetChunkBlocksToStatData(StatData* statData, size_t count)
{
   Random rand;

   for(size_t i = 0; i < count; i++)
   {
      uint64_t value = rand.getNextInt();
      statData->setTargetChunkBlocks(i, value, count);
   }
}
