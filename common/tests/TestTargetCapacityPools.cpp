#include <common/nodes/TargetCapacityPools.h>

#include <gtest/gtest.h>

TEST(TargetCapacityPools, interdomainWithEmptyGroups)
{
   TargetCapacityPools pools(false, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});

   pools.addOrUpdate(1, NumNodeID(1), CapacityPool_NORMAL);
   // moves target from LOW to NORMAL, must remove the NORMAL group from chooser
   pools.addOrUpdate(1, NumNodeID(1), CapacityPool_LOW);

   std::vector<uint16_t> chosen;
   pools.chooseTargetsInterdomain(4, 1, &chosen);

   EXPECT_EQ(chosen.size(), 1u);
   ASSERT_EQ(chosen[0], 1);
}
