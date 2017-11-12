#ifndef SIMPLEPATTERN_H_
#define SIMPLEPATTERN_H_

#include "StripePattern.h"


/**
 * This pattern exists primarily to allow easy creation of a dummy pattern that can have type
 * STRIPEPATTERN_Invalid if anything goes wrong.
 */
class SimplePattern : public StripePattern
{
   public:
      SimplePattern(unsigned patternType, unsigned chunkSize) :
         StripePattern(patternType, chunkSize)
      {
      }

      virtual StripePattern* clone() const
      {
         SimplePattern* pattern = new SimplePattern(getPatternType(), getChunkSize() );
         
         return pattern;
      }
      
      virtual StripePattern* clone(const UInt16Vector& stripeTargetIDs) const
      {
         return clone();
      }
      
   protected:

      virtual void serializePattern(char* buf)
      {
         // nothing to be done here
      }

      virtual bool deserializePattern(const char* buf, size_t bufLen)
      {
         return true;
      }

      virtual unsigned serialLen()
      {
         return STRIPEPATTERN_HEADER_LENGTH;
      }
      
   public:
      // getters & setters
      virtual size_t getStripeTargetIndex(int64_t pos) const
      {
         return 0;
      };

      virtual uint16_t getStripeTargetID(int64_t pos) const
      {
         return 0;
      };
      
      virtual void getStripeTargetIDs(UInt16Vector* outTargetIDs) const
      {
         // nothing to be done here
      }
      
      virtual const UInt16Vector* getStripeTargetIDs() const
      {
         return NULL;
      }

      virtual UInt16Vector* getStripeTargetIDsModifyable()
      {
         return NULL;
      }

      virtual unsigned getMinNumTargets() const
      {
         return 0;
      }

      virtual unsigned getDefaultNumTargets() const
      {
         return 0;
      }

      virtual unsigned getDefaultNumTargetsWithRedundancy() const
      {
         return 0;
      }

      virtual bool updateStripeTargetIDs(StripePattern* updatedStripePattern)
      {
         return true;
      }
};

#endif /*SIMPLEPATTERN_H_*/
