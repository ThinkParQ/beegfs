#ifndef DYNAMICPOOLLIMITS_H_
#define DYNAMICPOOLLIMITS_H_

#include <common/toolkit/MinMaxStore.h>
#include <common/nodes/CapacityPoolType.h>

#include <stdint.h>

class DynamicPoolLimits
{
   public:
      DynamicPoolLimits(int64_t lowLimit, int64_t emergencyLimit,
         int64_t normalSpreadThreshold, int64_t lowSpreadThreshold,
         int64_t lowDynamicLimit, int64_t emergencyDynamicLimit) :
            lowLimit(lowLimit), emergencyLimit(emergencyLimit),
               normalSpreadThreshold(normalSpreadThreshold), lowSpreadThreshold(lowSpreadThreshold),
               lowDynamicLimit(lowDynamicLimit), emergencyDynamicLimit(emergencyDynamicLimit)
      { }

   private:
      const int64_t lowLimit;
      const int64_t emergencyLimit;
      const int64_t normalSpreadThreshold;
      const int64_t lowSpreadThreshold;
      const int64_t lowDynamicLimit;
      const int64_t emergencyDynamicLimit;

   public:
      // getters
      int64_t getLowLimit() const
      {
         return this->lowLimit;
      }

      int64_t getEmergencyLimit() const
      {
         return this->emergencyLimit;
      }

      int64_t getNormalSpreadThreshold() const
      {
         return this->normalSpreadThreshold;
      }

      int64_t getLowSpreadThreshold() const
      {
         return this->lowSpreadThreshold;
      }

      int64_t getLowDynamicLimit() const
      {
         return this->lowDynamicLimit;
      }

      int64_t getEmergencyDynamicLimit() const
      {
         return this->emergencyDynamicLimit;
      }


      // inliners
      CapacityPoolType getPoolTypeFromFreeCapacity(int64_t freeCapacity) const
      {
         if(freeCapacity > this->lowLimit)
            return CapacityPool_NORMAL;

         if(freeCapacity > this->emergencyLimit)
            return CapacityPool_LOW;

         return CapacityPool_EMERGENCY;
      }

      /**
       * @return whether the dynamically raised limit is active for the "NORMAL" pool, determined
       *         from the given minimum/maximum free space and the normalSpreadThreshold
       */
      bool demotionActiveNormalPool(const MinMaxStore<int64_t>& freeMinMax) const
      {
         int64_t spread = freeMinMax.getMax() - freeMinMax.getMin();
         return spread > this->normalSpreadThreshold;
      }

      /**
       * @return whether the dynamically raised limit is active for the "LOW" pool, determined from
       *         the given minimum/maximum free space and the lowSpreadThreshold
       */
      bool demotionActiveLowPool(const MinMaxStore<int64_t>& freeMinMax) const
      {
         int64_t spread = freeMinMax.getMax() - freeMinMax.getMin();
         return spread > this->lowSpreadThreshold;
      }

      /**
       * @return whether a target currently assigned to the NORMAL pool should be demoted to
       *         the LOW pool under the assumption that demotion is active.
       */
      bool demoteNormalToLow(int64_t free) const
      {
         return free <= this->lowDynamicLimit;
      }

      /**
       * @return whether a target currently assigned to the LOW pool should be demoted to
       *         the EMERGENCY pool under the assumption that demotion is active.
       */
      bool demoteLowToEmergency(int64_t free) const
      {
         return free <= this->emergencyDynamicLimit;
      }
};

#endif // DYNAMICPOOLLIMITS_H_
