#ifndef SIMPLEPATTERN_H_
#define SIMPLEPATTERN_H_

#include "StripePattern.h"

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
      
      virtual StripePattern* clone(const StringVector& stripeTargetIDs) const
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

      virtual std::string getStripeTargetID(int64_t pos) const
      {
         return "";
      };
      
      virtual unsigned getMinNumTargets() const
      {
         return 0;
      }
      
      virtual unsigned getDefaultNumTargets() const
      {
         return 0;
      }
      
      virtual void getStripeTargetIDs(StringVector* outTargetIDs) const
      {
         // nothing to be done here
      }
      
      virtual StringVector* getStripeTargetIDs()
      {
         return NULL;
      }
};

#endif /*SIMPLEPATTERN_H_*/
