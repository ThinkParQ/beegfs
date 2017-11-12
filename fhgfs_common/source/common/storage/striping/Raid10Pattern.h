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
         StoragePoolId storagePoolId = StoragePoolStore::DEFAULT_POOL_ID):
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
         return cloneDisjoint(targetIDs);
      }

      /**
       * Note: This needs an even number of targetIDs, otherwise the last target in the list will
       * be dropped.
       * Example: targetIDs=1,2,3,4 => stripeTargetIDs=1,2; mirrorTargetIDs=3,4
       *
       * @param targetIDs will be rounded down to an even number; will be split internally into
       * two halfs (first half stripe targets, second half mirror targets).
       */
      StripePattern* cloneDisjoint(const UInt16Vector& targetIDs) const
      {
         // use the deserialization constructor here for fast swapping of vectors instead of copying
         /* (note: normal constructor cannot swap because that would break some callers which still
            use the stripe vectors afterwards). */

         Raid10Pattern* clonedPattern = new Raid10Pattern(getChunkSize() );

         // first half of targetIDs becomes normal stripe targets
         UInt16Vector stripeTargets(
            targetIDs.begin(), targetIDs.begin() + (targetIDs.size() / 2) );

         // second half of targetIDs becomes mirror targets
         UInt16Vector mirrorTargets(
            targetIDs.begin() + (targetIDs.size() / 2), targetIDs.end() );

         if(mirrorTargets.size() > stripeTargets.size() )
            mirrorTargets.pop_back(); // we need an equal number of stripe/mirror targets for RAID10

         // swap target vectors
         clonedPattern->stripeTargetIDs.swap(stripeTargets);
         clonedPattern->mirrorTargetIDs.swap(mirrorTargets);


         clonedPattern->defaultNumTargets = defaultNumTargets;

         return clonedPattern;
      }

      /**
       * Note: The advantage here is that we can use an odd number of stripe targets.
       * Example: targetIDs=1,2,3,4,5 => stripeTargetIDs=1,2,3,4,5; mirrorTargetIDs=3,4,5,1,2
       *
       * @param targetIDs mirrors will be targetIDs rotated by half vector size (so mirrored chunks
       * will be stored on the same targets, not on separate targets).
       */
      StripePattern* cloneRotated(const UInt16Vector& targetIDs) const
      {
         // use the deserialization constructor here for fast swapping of vectors instead of copying
         /* (note: normal constructor cannot swap because that would break some callers which still
            use the stripe vectors afterwards). */

         Raid10Pattern* clonedPattern = new Raid10Pattern(getChunkSize() );

         // stripeTargets is the given list of targetIDs
         UInt16Vector stripeTargets(targetIDs);

         // mirrorTargets is the given list of targetIDs rotated by half vector size
         UInt16Vector mirrorTargets;
         mirrorTargets.reserve(targetIDs.size() );

         // insert second half of targetIDs vector
         mirrorTargets.insert(mirrorTargets.end(),
            targetIDs.begin() + (targetIDs.size() / 2), targetIDs.end() );

         // insert first half of targetIDs vector
         mirrorTargets.insert(mirrorTargets.end(),
            targetIDs.begin(), targetIDs.begin() + (targetIDs.size() / 2) );

         // swap target vectors
         clonedPattern->stripeTargetIDs.swap(stripeTargets);
         clonedPattern->mirrorTargetIDs.swap(mirrorTargets);


         clonedPattern->defaultNumTargets = defaultNumTargets;

         return clonedPattern;
      }


      // static inliners

      static unsigned getMinNumTargetsStatic()
      {
         return 2;
      }

};

#endif /* RAID10PATTERN_H_ */
