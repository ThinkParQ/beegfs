#ifndef MODEIOTEST_H_
#define MODEIOTEST_H_

#include <common/Common.h>
#include <common/storage/PathInfo.h>
#include <common/storage/striping/StripePattern.h>
#include "Mode.h"


class ModeIOTest : public Mode
{
   public:
      ModeIOTest()
      {
         cfgWrite = false;
         cfgRead = false;
         cfgFilesize = 128*1024*1024;
         cfgRecordsize = 1024*1024;
         cfgIntermediateSecs = 10;
      }

      virtual int execute();

      static void printHelp();


   private:
      bool cfgWrite;
      bool cfgRead;
      uint64_t cfgFilesize;
      unsigned cfgRecordsize;
      unsigned cfgIntermediateSecs;

      bool writeTest(Node& ownerNode, EntryInfo* entryInfo);
      bool readTest(Node& ownerNode, EntryInfo* entryInfo);
      bool openFile(Node& ownerNode, EntryInfo* entryInfo, unsigned openFlags,
         std::string* outFileHandleID, StripePattern** outPattern, PathInfo* outPathInfo);
      bool closeFile(Node& ownerNode, std::string fileHandleID, EntryInfo* entryInfo,
         int maxUsedNodeIndex);
      ssize_t writefile(const char *buf, size_t size, off_t offset,
         std::string fileHandleID,  unsigned accessFlags, StripePattern* pattern,
         PathInfo* pathInfo);
      ssize_t readfile(char *buf, size_t size, off_t offset, std::string fileHandleID,
         unsigned accessFlags, StripePattern* pattern, PathInfo* pathInfo);
      long long getNodeLocalOffset(long long pos, long long chunkSize, size_t numTargets,
         size_t stripeNodeIndex);
      void printPattern(StripePattern* pattern);

};

#endif /* MODEIOTEST_H_ */
