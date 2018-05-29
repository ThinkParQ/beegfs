#ifndef BUDDYMIRRORPATTERN_H_
#define BUDDYMIRRORPATTERN_H_

#include "StripePattern.h"

#include <common/nodes/StoragePoolStore.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/MathTk.h>


#define BUDDYMIRRORPATTERN_DEFAULT_NUM_TARGETS  4

class BuddyMirrorPattern : public StripePattern
{
   friend class StripePattern;

   public:
      /**
       * @param chunkSize 0 for app-level default
       * @param defaultNumTargets default number of mirror buddy groups (0 for app-level default)
       * @param storagePoolId
       */
      BuddyMirrorPattern(unsigned chunkSize, const UInt16Vector& mirrorBuddyGroupIDs,
         unsigned defaultNumTargets = 0,
         StoragePoolId storagePoolId = StoragePoolStore::DEFAULT_POOL_ID):
         StripePattern(StripePatternType_BuddyMirror, chunkSize, storagePoolId),
         mirrorBuddyGroupIDs(mirrorBuddyGroupIDs)
      {
         this->defaultNumTargets =
            defaultNumTargets ? defaultNumTargets : BUDDYMIRRORPATTERN_DEFAULT_NUM_TARGETS;
      }

   protected:
      /**
       * Note: for deserialization only
       */
      BuddyMirrorPattern(unsigned chunkSize,
         boost::optional<StoragePoolId> storagePoolId = StoragePoolStore::DEFAULT_POOL_ID):
         StripePattern(StripePatternType_BuddyMirror, chunkSize, storagePoolId) { }

      // (de)serialization

      template<typename This, typename Ctx>
      static void serializeContent(This obj, Ctx& ctx)
      {
         ctx
            % obj->defaultNumTargets
            % obj->mirrorBuddyGroupIDs;
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
      UInt16Vector mirrorBuddyGroupIDs;
      uint32_t defaultNumTargets;

   public:
      // getters & setters

      virtual const UInt16Vector* getStripeTargetIDs() const
      {
         return &mirrorBuddyGroupIDs;
      }

      virtual UInt16Vector* getStripeTargetIDsModifyable()
      {
         return &mirrorBuddyGroupIDs;
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
         return mirrorBuddyGroupIDs.size();
      }


      // inliners

      virtual StripePattern* clone() const
      {
         return new auto(*this);
      }


      // static inliners

      static unsigned getMinNumTargetsStatic()
      {
         return 1;
      }

};

#endif /*BUDDYMIRRORPATTERN_H_*/
