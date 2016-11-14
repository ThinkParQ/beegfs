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
         StripePattern(StripePatternType_Raid0, chunkSize), stripeTargetIDs(stripeTargetIDs)
      {
         this->defaultNumTargets =
            defaultNumTargets ? defaultNumTargets : RAID0PATTERN_DEFAULT_NUM_TARGETS;
      }

   protected:
      /**
       * Note: for deserialization only
       */
      Raid0Pattern(unsigned chunkSize) : StripePattern(StripePatternType_Raid0, chunkSize)
      {
         // nothing to be done here
      }

      // (de)serialization

      template<typename This, typename Ctx>
      static void serializeContent(This obj, Ctx& ctx)
      {
         ctx
            % obj->defaultNumTargets
            % obj->stripeTargetIDs;
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
       * Number of targets actually assigned
       */
      virtual size_t getAssignedNumTargets() const
      {
         return stripeTargetIDs.size();
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
