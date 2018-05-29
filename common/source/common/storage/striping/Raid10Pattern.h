#ifndef RAID10PATTERN_H_
#define RAID10PATTERN_H_

#include <common/app/log/LogContext.h>
#include <common/nodes/StoragePoolStore.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/MathTk.h>

#include "StripePattern.h"


#define RAID10PATTERN_DEFAULT_NUM_TARGETS  4


class Raid10Pattern : public StripePattern
{
   friend class StripePattern;

   public:
      /**
       * @param chunkSize 0 for app-level default
       * @param mirrorTargetIDs caller is responsible for ensuring that length of stripeTargetIDs
       * and length of mirrorTargetIDs are equal.
       * @param defaultNumTargets default number of targets (0 for app-level default)
       */
      Raid10Pattern(unsigned chunkSize, const UInt16Vector& stripeTargetIDs,
         const UInt16Vector& mirrorTargetIDs, unsigned defaultNumTargets=0,
         StoragePoolId storagePoolId = StoragePoolStore::DEFAULT_POOL_ID):
         StripePattern(StripePatternType_Raid10, chunkSize, storagePoolId),
         stripeTargetIDs(stripeTargetIDs), mirrorTargetIDs(mirrorTargetIDs)
      {
         this->defaultNumTargets =
            defaultNumTargets ? defaultNumTargets : RAID10PATTERN_DEFAULT_NUM_TARGETS;

         // sanity check for equal vector lengths...

         #ifdef BEEGFS_DEBUG
            if(unlikely(stripeTargetIDs.size() != mirrorTargetIDs.size() ) )
            {
               LogContext(__func__).logErr("Unexpected: Vectors lengths differ: "
                  "stripeTargetIDs: " + StringTk::uintToStr(stripeTargetIDs.size() ) + "; "
                  "mirrorTargetIDs: " + StringTk::uintToStr(mirrorTargetIDs.size() ) );
               LogContext(__func__).logBacktrace();
            }
         #endif // BEEGFS_DEBUG

      }


   protected:
      /**
       * Note: for deserialization only
       */
      Raid10Pattern(unsigned chunkSize,
         boost::optional<StoragePoolId> storagePoolId = StoragePoolStore::DEFAULT_POOL_ID):
         StripePattern(StripePatternType_Raid10, chunkSize, storagePoolId) { }

      // (de)serialization

      template<typename This, typename Ctx>
      static void serializeContent(This obj, Ctx& ctx)
      {
         ctx
            % obj->defaultNumTargets
            % obj->stripeTargetIDs
            % obj->mirrorTargetIDs;
      }

      virtual void serializeContent(Serializer& ser) const
      {
         serializeContent(this, ser);
      }

      virtual void deserializeContent(Deserializer& des)
      {
         serializeContent(this, des);
      }

      virtual bool patternEquals(const StripePattern* second) const;

   private:
      UInt16Vector stripeTargetIDs;
      UInt16Vector mirrorTargetIDs;
      uint32_t defaultNumTargets;


   public:
      // getters & setters

      virtual const UInt16Vector* getStripeTargetIDs() const
      {
         return &stripeTargetIDs;
      }

      virtual UInt16Vector* getStripeTargetIDsModifyable()
      {
         return &stripeTargetIDs;
      }

      virtual bool updateStripeTargetIDs(StripePattern* updatedStripePattern);

      virtual const UInt16Vector* getMirrorTargetIDs() const
      {
         return &mirrorTargetIDs;
      }

      virtual UInt16Vector* getMirrorTargetIDsModifyable()
      {
         return &mirrorTargetIDs;
      }

      virtual unsigned getMinNumTargets() const
      {
         return getMinNumTargetsStatic();
      }

      virtual unsigned getDefaultNumTargets() const
      {
         return defaultNumTargets;
      }

      virtual void setDefaultNumTargets(unsigned defaultNumTargets)
      {
         this->defaultNumTargets = defaultNumTargets;
      }

      /**
       * Number of targets actually assigned as raid0 (excluding mirroring)
       */
      virtual size_t getAssignedNumTargets() const
      {
         return stripeTargetIDs.size();
      }

      // inliners

      virtual StripePattern* clone() const
      {
         Raid10Pattern* clonedPattern = new Raid10Pattern(
            getChunkSize(), stripeTargetIDs, mirrorTargetIDs, defaultNumTargets);

         return clonedPattern;
      }

      StripePattern* clone(const UInt16Vector& targetIDs) const
      {
         return new auto(*this);
      }


      // static inliners

      static unsigned getMinNumTargetsStatic()
      {
         return 2;
      }

};

#endif /* RAID10PATTERN_H_ */
