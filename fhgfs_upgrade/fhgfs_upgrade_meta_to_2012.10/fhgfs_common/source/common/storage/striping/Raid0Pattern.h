#ifndef RAID0PATTERN_H_
#define RAID0PATTERN_H_

#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/MathTk.h>
#include "StripePattern.h"


#define RAID0PATTERN_DEFAULT_NUM_TARGETS  4


class Raid0Pattern : public StripePattern
{
   friend class StripePattern;
   
   public:
      /**
       * @param chunkSize 0 for app-level default
       * @param defaultNumTargets default number of targets (0 for app-level default)
       */
      Raid0Pattern(unsigned chunkSize, const UInt16Vector& stripeTargetIDs,
         unsigned defaultNumTargets=0) :
         StripePattern(STRIPEPATTERN_Raid0, chunkSize), stripeTargetIDs(stripeTargetIDs)
      {
         // this->stripeTargetIDs = stripeTargetIDs; // (done in initializer list now)

         this->defaultNumTargets =
            defaultNumTargets ? defaultNumTargets : RAID0PATTERN_DEFAULT_NUM_TARGETS;

         this->stripeSetSize = stripeTargetIDs.size() * getChunkSize(); /* note: chunkSize might
            change in super class contructor, so we need to re-get the value here */
      }

      bool mapStringIDs(StringUnsignedMap* idMap);

      
   protected:
      /**
       * Note: for deserialization only
       */
      Raid0Pattern(unsigned chunkSize) : StripePattern(STRIPEPATTERN_Raid0, chunkSize)
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
            Serialization::serialLenUInt16Vector(&stripeTargetIDs);
      };
      

   private:
      UInt16Vector stripeTargetIDs;
      StringVector stripeTargetIDsStrVec; // for upgrade tool

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
         return clone(stripeTargetIDs);
      }

      StripePattern* clone(const UInt16Vector& targetIDs) const
      {
         Raid0Pattern* pattern = new Raid0Pattern(getChunkSize(), targetIDs,
            defaultNumTargets);

         return pattern;
      }


      // static inliners

      static unsigned getMinNumTargetsStatic()
      {
         return 1;
      }

};

#endif /*RAID0PATTERN_H_*/
