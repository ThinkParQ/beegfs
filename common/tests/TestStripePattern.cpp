#include <common/storage/striping/StripePattern.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <vector>

#include <gtest/gtest.h>


class Raid0PatternTest : public testing::TestWithParam<unsigned>
{
};

INSTANTIATE_TEST_CASE_P(Name, Raid0PatternTest,
      ::testing::Values(
         64*1024,
         1024*1024*1024
         )
      );

TEST_P(Raid0PatternTest, chunkSizes)
{
   const auto chunkSize = GetParam();
   const auto targetPattern =  std::vector<uint16_t>({0,1,2,3});
   const Raid0Pattern p(chunkSize, targetPattern);

   ASSERT_EQ(p.getChunkSize(), chunkSize);

   for(size_t i=0; i<10*targetPattern.size(); ++i) {

      const auto chunkPos = int64_t(i) * chunkSize;
      const auto chunkEnd = chunkPos + chunkSize -1;

      ASSERT_EQ(p.getStripeTargetIndex(chunkPos), i % targetPattern.size());
      ASSERT_EQ(p.getStripeTargetIndex(chunkEnd), i % targetPattern.size());


      ASSERT_EQ(p.getChunkStart(chunkPos), chunkPos);
      ASSERT_EQ(p.getChunkStart(chunkEnd), chunkPos);
   }
}

