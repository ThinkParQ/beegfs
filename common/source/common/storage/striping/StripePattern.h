#ifndef STRIPEPATTERN_H_
#define STRIPEPATTERN_H_

#include <boost/optional.hpp>
#include <common/Common.h>
#include <common/nodes/StoragePoolStore.h>
#include <common/toolkit/serialization/Serialization.h>

#define STRIPEPATTERN_MIN_CHUNKSIZE       (1024*64) /* minimum allowed stripe pattern chunk size */
#define STRIPEPATTERN_DEFAULT_CHUNKSIZE   (1024*512)

enum StripePatternType
{
   // 0 was used to indicate invalid stripe patterns before, don't reuse that
   StripePatternType_Invalid,
   StripePatternType_Raid0,
   StripePatternType_Raid10,
   StripePatternType_BuddyMirror,
};

struct StripePatternHeader
{
   StripePatternHeader(bool hasPoolId) :
      hasPoolId(hasPoolId)
   {
      if (!hasPoolId)
         storagePoolId = StoragePoolStore::DEFAULT_POOL_ID;
   }

   StripePatternHeader(StripePatternType type, uint32_t chunkSize, boost::optional<StoragePoolId> storagePoolId) :
        length(0), type(type), chunkSize(chunkSize),
        storagePoolId(storagePoolId.value_or(StoragePoolStore::DEFAULT_POOL_ID)),
        hasPoolId(!!storagePoolId)
        {}

   uint32_t length;
   StripePatternType type;
   uint32_t chunkSize;
   StoragePoolId storagePoolId;
   bool hasPoolId; // used for deserialization to be compatible with v6 format

   // we use some of the high bits of type to store flags in the binary format.
   // HasNoPool indicates that the stripe pattern has no pool id in it, even if hasPoolId is true.
   // this is required to correctly transport stripe patterns without pool ids across the network.
   // in 7.0 release a few things happened:
   //  * the client always deserializes a pool id.
   //  * for patterns read from inodes the server serializes a pool id only if it was present in
   //    the inode
   //  * pattern deserialized from scratch in userspace always deserialize a pool id
   //
   // old metadata formats (eg 2014) do not contain pool ids. the server will then not serialize a
   // pool id, but both clients and tools will expect one. we thus need a flag to indicate that no
   // pool id is present. using a flag to indicate that a pool id *is* present does not work because
   // "present" is the default and must be overridden in special cases.
   //
   // this is a change to the disk and wire format that has no effect on the in-memory layout of
   // stripe patterns in userspace and client if the pattern is read correctly:
   //  * from an inode with a pool id: the flag will never be set
   //  * from an inode without a pool id:
   //    * if the inode was not updated, nothing has changed
   //    * if the inode was updated the NoPool flag is set and has no effect.
   //  * from a buffer with a pool id: the flag will not be set
   //  * from a buffer without a pool id: the flag will be set and allow deserialization at all.
   //    deserializers of 7.0 rejected the pattern as erroneous in this case.
   static constexpr uint32_t HasNoPoolFlag = 1 << 24;

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx % obj->length;

      struct {
         void operator()(const StripePatternHeader* obj, Serializer& ctx) {
            uint32_t typeWithFlags = uint32_t(obj->type) | (obj->hasPoolId ? 0 : HasNoPoolFlag);

            ctx % typeWithFlags;
         }

         void operator()(StripePatternHeader* obj, Deserializer& ctx) {
            uint32_t typeWithFlags;

            ctx % typeWithFlags;

            obj->hasPoolId &= !(typeWithFlags & HasNoPoolFlag);
            obj->type = StripePatternType(typeWithFlags & ~HasNoPoolFlag);
         }
      } chunkTypeAndFlags;
      chunkTypeAndFlags(obj, ctx);

      ctx % obj->chunkSize;

      if (obj->hasPoolId)
         ctx % obj->storagePoolId;
   }
};

class StripePattern
{
   friend class DiskMetaData;

   public:
      virtual ~StripePattern() {}

      static std::string getPatternTypeStr(StripePatternType patternType);


      std::string getPatternTypeStr() const;


      /**
       * Creates a clone of the pattern.
       *
       * Note: The clone must be deleted by the caller.
       * Note: See derived classes implement more clone methods with additional args.
       */
      virtual StripePattern* clone() const = 0;

      bool stripePatternEquals(const StripePattern* second) const;

   protected:
      /**
       * @param chunkSize 0 for app-level default
       */
      StripePattern(StripePatternType type, unsigned chunkSize,
            boost::optional<StoragePoolId> storagePoolId)
         : header(type, chunkSize ? chunkSize : STRIPEPATTERN_DEFAULT_CHUNKSIZE, storagePoolId) { }

      // (de)serialization
      static StripePattern* deserialize(Deserializer& des, bool hasPoolId);

      virtual void serializeContent(Serializer& ser) const = 0;
      virtual void deserializeContent(Deserializer& des) = 0;

      virtual bool patternEquals(const StripePattern* second) const = 0;

   protected:
      StripePatternHeader header;

   public:
      // getters & setters

      StripePatternType getPatternType() const
      {
         return header.type;
      }

      unsigned getChunkSize() const
      {
         return header.chunkSize;
      }

      void setChunkSize(unsigned chunkSize)
      {
         this->header.chunkSize = chunkSize;
      }

      StoragePoolId getStoragePoolId() const
      {
         return header.storagePoolId;
      }

      void setStoragePoolId(StoragePoolId storagePoolId)
      {
         header.storagePoolId = storagePoolId;
         header.hasPoolId = true;
      }

      int64_t getChunkStart(int64_t pos) const
      {
         // the code below is an optimization (wrt division) for the following line:
         //    int64_t chunkStart = pos - (pos % this->chunkSize);

         // "& chunkSize -1" instead of "%", because chunkSize is a power of two
         unsigned posModChunkSize = pos & (this->header.chunkSize - 1);

         int64_t chunkStart = pos - posModChunkSize;

         return chunkStart;
      }

      int64_t getNextChunkStart(int64_t pos) const
      {
         return getChunkStart(pos) + this->header.chunkSize;
      }

      int64_t getChunkEnd(int64_t pos) const
      {
         return getNextChunkStart(pos) - 1;
      }

      size_t getNumStripeTargetIDs() const
      {
         return getStripeTargetIDs()->size();
      }

      /**
       * Get index of target in stripe vector for given file offset.
       */
      size_t getStripeTargetIndex(int64_t pos) const
      {
         return (pos / getChunkSize()) % getNumStripeTargetIDs();
      }

      /**
       * Get targetID for given file offset.
       */
      uint16_t getStripeTargetID(int64_t pos) const
      {
         size_t targetIndex = getStripeTargetIndex(pos);

         return (*getStripeTargetIDs() )[targetIndex];
      }

      /**
       * Note: this is called quite often, so we have a const version to enable better compiler
       * code optimizations. (there are some special cases where targetIDs need to be modified, so
       * we also have the non-const/modifyable version.)
       */
      virtual const UInt16Vector* getStripeTargetIDs() const = 0;

      /**
       * Note: Rather use the const version (getStripeTargetIDs), if the siutation allows it. This
       * method is only for special use cases, as the stripe targets usually don't change after
       * pattern object creation.
       */
      virtual UInt16Vector* getStripeTargetIDsModifyable() = 0;

      /**
       * Note: Use carefully; this is only for very special use-cases (e.g. fsck/repair) because
       * stripe targets are assumed to be immutable after pattern instantiation.
       */
      virtual bool updateStripeTargetIDs(StripePattern* updatedStripePattern) = 0;

      /**
       * Predefined virtual method returning NULL. Will be overridden by StripePatterns
       * (e.g. Raid10) that actually do have mirror targets.
       *
       * @return NULL for patterns that don't have mirror targets.
       */
      virtual const UInt16Vector* getMirrorTargetIDs() const
      {
         return NULL;
      }

      /**
       * The minimum number of targets required to use this pattern.
       */
      virtual unsigned getMinNumTargets() const = 0;

      /**
       * The desired number of targets that was given when this pattern object was created.
       * (Mostly used for directories to configure how many targets a new file within the diretory
       * shold have.)
       *
       * For patterns with redundancy, this is the number includes redundancy targets.
       */
      virtual unsigned getDefaultNumTargets() const = 0;

      virtual void setDefaultNumTargets(unsigned defaultNumTargets) = 0;

      /**
       * Number of targets actually assigned as raid0
       */
      virtual size_t getAssignedNumTargets() const = 0;



      friend Serializer& operator%(Serializer& ser, const StripePattern& pattern)
      {
         Serializer lengthPos = ser.mark();
         unsigned lengthAtStart = ser.size();

         ser % pattern.header;

         pattern.serializeContent(ser);

         lengthPos % (ser.size() - lengthAtStart);

         return ser;
      }

      friend Serializer& operator%(Serializer& ser, const StripePattern* pattern)
      {
         return ser % *pattern;
      }

      friend Serializer& operator%(Serializer& ser, const std::unique_ptr<StripePattern>& pattern)
      {
         return ser % *pattern;
      }

      friend Deserializer& operator%(Deserializer& des, StripePattern*& pattern)
      {
         delete pattern;
         pattern = NULL;

         pattern = StripePattern::deserialize(des, true);

         return des;
      }

      friend Deserializer& operator%(Deserializer& des, std::unique_ptr<StripePattern>& pattern)
      {
         pattern.reset(StripePattern::deserialize(des, true));
         return des;
      }
};

#endif /*STRIPEPATTERN_H_*/
