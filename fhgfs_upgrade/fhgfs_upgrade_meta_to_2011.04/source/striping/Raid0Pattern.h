#ifndef RAID0PATTERN_H_
#define RAID0PATTERN_H_

#include <serialization/Serialization.h>
#include "StripePattern.h"


class Raid0Pattern : public StripePattern
{
   friend class StripePattern;
   
   public:
      /**
       * @param chunkSize 0 for app-level default
       * @param defaultNumTargets default number of targets (0 for app-level default)
       */
      Raid0Pattern(unsigned chunkSize, const StringVector& stripeTargetIDs,
         unsigned defaultNumTargets=0) :
         StripePattern(STRIPEPATTERN_Raid0, chunkSize)
      {
         this->stripeTargetIDs = stripeTargetIDs;

         this->defaultNumTargets = defaultNumTargets ? defaultNumTargets : 4;

         this->stripeSetSize = stripeTargetIDs.size() * chunkSize;
      }
      
      virtual StripePattern* clone() const
      {
         return clone(stripeTargetIDs);
      }
      
      virtual StripePattern* clone(const StringVector& stripeTargetIDs) const
      {
         Raid0Pattern* pattern = new Raid0Pattern(getChunkSize(), stripeTargetIDs,
            defaultNumTargets);
         
         return pattern;
      }

      static unsigned getMinNumTargetsStatic() 
      {
         return 1;
      }
      
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
            Serialization::serialLenStringVec(&stripeTargetIDs) + // stripeTargetIDs
            Serialization::serialLenUInt(); // defaultNumTargets
      };
      
   private:
      StringVector stripeTargetIDs;
      unsigned stripeSetSize; // = numStripeTargets * chunkSize
      unsigned defaultNumTargets;
      
   public:
      // getters & setters
      virtual size_t getStripeTargetIndex(int64_t pos) const
      {
         int64_t stripeSetInnerOffset = pos % stripeSetSize;
         int64_t targetIndex = stripeSetInnerOffset / getChunkSize();
         
         return (unsigned)targetIndex;
      }

      virtual std::string getStripeTargetID(int64_t pos) const
      {
         size_t targetIndex = getStripeTargetIndex(pos);
         
         return stripeTargetIDs[targetIndex];
      }
      
      virtual unsigned getMinNumTargets() const
      {
         return Raid0Pattern::getMinNumTargetsStatic();
      }
      
      virtual unsigned getDefaultNumTargets() const
      {
         return defaultNumTargets;
      }
      
      virtual void getStripeTargetIDs(StringVector* outTargetIDs) const
      {
         *outTargetIDs = stripeTargetIDs;
      }

      virtual StringVector* getStripeTargetIDs()
      {
         return &stripeTargetIDs;
      }

      
};

#endif /*RAID0PATTERN_H_*/
