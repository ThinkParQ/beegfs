#ifndef RAID10PATTERN_H_
#define RAID10PATTERN_H_

#include <common/app/log/LogContext.h>
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
         const UInt16Vector& mirrorTargetIDs, unsigned defaultNumTargets=0) :
         StripePattern(STRIPEPATTERN_Raid10, chunkSize),
         stripeTargetIDs(stripeTargetIDs), mirrorTargetIDs(mirrorTargetIDs)
      {
         // this->stripeTargetIDs = stripeTargetIDs; // (done in initializer list now)
         // this->mirrorTargetIDs = mirrorTargetIDs; // (done in initializer list now)

         this->defaultNumTargets =
            defaultNumTargets ? defaultNumTargets : RAID10PATTERN_DEFAULT_NUM_TARGETS;

         this->stripeSetSize = stripeTargetIDs.size() * getChunkSize(); /* note: chunkSize might
            change in super class contructor, so we need to re-get the value here */

         // sanity check for equal vector lengths...

         #ifdef FHGFS_DEBUG
            if(unlikely(stripeTargetIDs.size() != mirrorTargetIDs.size() ) )
            {
               LogContext(__func__).logErr("Unexpected: Vectors lengths differ: "
                  "stripeTargetIDs: " + StringTk::uintToStr(stripeTargetIDs.size() ) + "; "
                  "mirrorTargetIDs: " + StringTk::uintToStr(mirrorTargetIDs.size() ) );
               LogContext(__func__).logBacktrace();
            }
         #endif // FHGFS_DEBUG

      }


   protected:
      /**
       * Note: for deserialization only
       */
      Raid10Pattern(unsigned chunkSize) : StripePattern(STRIPEPATTERN_Raid10, chunkSize)
      {
         // nothing to be done here
      }

      // (de)serialization

      virtual void serializePattern(char* buf);
      virtual bool deserializePattern(const char* buf, size_t bufLen);

      virtual unsigned serialLen()
      {
         return STRIPEPATTERN_HEADER_LENGTH +
            Serialization::serialLenUInt() + // defaultNumTargets
            Serialization::serialLenUInt16Vector(&stripeTargetIDs) +
            Serialization::serialLenUInt16Vector(&mirrorTargetIDs);
      };


   private:
      UInt16Vector stripeTargetIDs;
      UInt16Vector mirrorTargetIDs;
      unsigned stripeSetSize; // = numStripeTargets * chunkSize
      unsigned defaultNumTargets;


   public:
      // getters & setters

      virtual size_t getStripeTargetIndex(int64_t pos) const
      {
         /* the code below is an optimization (wrt division) of the following two lines:
               int64_t stripeSetInnerOffset = pos % stripeSetSize;
               int64_t targetIndex = stripeSetInnerOffset / StripePattern_getChunkSize(this); */


         int64_t stripeSetInnerOffset;

         if(MathTk::isPowerOfTwo(stripeSetSize) )
         { // quick path => no modulo needed
            stripeSetInnerOffset = pos & (stripeSetSize - 1);
         }
         else
         { // slow path => modulo
            stripeSetInnerOffset = pos % stripeSetSize;
         }

         unsigned chunkSize = getChunkSize();

         // this is "a=b/c" written as "a=b>>log2(c)", because chunkSize is a power of two.
         size_t targetIndex = (stripeSetInnerOffset >> MathTk::log2Int32(chunkSize) );

         return targetIndex;
      }

      virtual uint16_t getStripeTargetID(int64_t pos) const
      {
         size_t targetIndex = getStripeTargetIndex(pos);

         return stripeTargetIDs[targetIndex];
      }

      virtual void getStripeTargetIDs(UInt16Vector* outTargetIDs) const
      {
         *outTargetIDs = stripeTargetIDs;
      }

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

         clonedPattern->stripeSetSize =
            clonedPattern->stripeTargetIDs.size() * clonedPattern->getChunkSize();

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

         clonedPattern->stripeSetSize =
            clonedPattern->stripeTargetIDs.size() * clonedPattern->getChunkSize();

         return clonedPattern;
      }


      // static inliners

      static unsigned getMinNumTargetsStatic()
      {
         return 2;
      }

};

#endif /* RAID10PATTERN_H_ */
