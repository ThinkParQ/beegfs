#ifndef STRIPEPATTERN_H_
#define STRIPEPATTERN_H_

#include <common/Common.h>


// pattern types
#define STRIPEPATTERN_Invalid 0
#define STRIPEPATTERN_Raid0   1
#define STRIPEPATTERN_Raid10  2

#define STRIPEPATTERN_MIN_CHUNKSIZE       (1024*64) /* minimum allowed stripe pattern chunk size */
#define STRIPEPATTERN_DEFAULT_CHUNKSIZE   (1024*512)


// pattern serialization defs
#define STRIPEPATTERN_HEADER_LENGTH \
   (sizeof(unsigned) + sizeof(unsigned) + sizeof(unsigned) ) /* length + type + chunkSize */


struct StripePatternHeader
{
   // everything in this struct is in host byte order!
   
   unsigned patternLength; // in bytes
   unsigned patternType; // the type of pattern, defined as STRIPEPATTERN_x
   unsigned chunkSize;
};



class StripePattern
{
   public:
      virtual ~StripePattern() {}


      static StripePattern* createFromBuf(const char* patternStart,
         struct StripePatternHeader& patternHeader);

      
      /**
       * Creates a clone of the pattern.
       *
       * Note: The clone must be deleted by the caller.
       * Note: See derived classes implement more clone methods with additional args.
       */ 
      virtual StripePattern* clone() const = 0;
      
      /**
       * Creates a clone of the pattern with new targets.
       *
       * @param targetIDs for patterns with redundancy (e.g. Raid10), this vector must also
       * contain the redundancy targets and will be split internally.
       */
      virtual StripePattern* clone(const UInt16Vector& targetIDs) const = 0;


      // inliners

      static bool deserializePatternPreprocess(const char* recvBuf, size_t bufLen,
         struct StripePatternHeader* outHeader, const char** outPatternStart,
         unsigned* outPatternLength)
      {
         if(!deserializeHeader(recvBuf, bufLen, outHeader) )
            return false;
         
         *outPatternStart = recvBuf;
         *outPatternLength = outHeader->patternLength;
         
         return true; 
      }

      unsigned serialize(char* buf)
      {
         serializeHeader(buf);
         serializePattern(&buf[STRIPEPATTERN_HEADER_LENGTH]);
         
         return getSerialPatternLength();
      }

   protected:
      /**
       * @param chunkSize 0 for app-level default
       */
      StripePattern(unsigned patternType, unsigned chunkSize)
      {
         this->patternType = patternType;
         this->chunkSize = chunkSize ? chunkSize : STRIPEPATTERN_DEFAULT_CHUNKSIZE;
         
         this->serialPatternLength = 0;
      }

      // (de)serialization

      virtual void serializePattern(char* buf) = 0;
      virtual bool deserializePattern(const char* buf, size_t bufLen) = 0;
      virtual unsigned serialLen() = 0;
      
      // getters & setters

      void setPatternType(unsigned patternType)
      {
         this->patternType = patternType;
      }
      

   private:
      unsigned patternType; // STRIPEPATTERN_...
      unsigned chunkSize; // must be a power of two (optimizations rely on it)
      
      unsigned serialPatternLength; // for (de)serialization

      
      static bool deserializeHeader(const char* buf, size_t bufLen,
         struct StripePatternHeader* outPatternHeader);

      void serializeHeader(char* buf);
      
      
   public:
      // getters & setters

      unsigned getPatternType() const
      {
         return patternType;
      }
      
      std::string getPatternTypeStr() const;

      unsigned getChunkSize() const
      {
         return chunkSize;
      }
      
      int64_t getChunkStart(int64_t pos) const
      {
         // the code below is an optimization (wrt division) for the following line:
         //    int64_t chunkStart = pos - (pos % this->chunkSize);

         // "& chunkSize -1" instead of "%", because chunkSize is a power of two
         unsigned posModChunkSize = pos & (this->chunkSize - 1);

         int64_t chunkStart = pos - posModChunkSize;
         
         return chunkStart;
      }

      int64_t getNextChunkStart(int64_t pos) const
      {
         return getChunkStart(pos) + this->chunkSize;
      }

      int64_t getChunkEnd(int64_t pos) const
      {
         return getNextChunkStart(pos) - 1; 
      }
      
      unsigned getSerialPatternLength()
      {
         if(!serialPatternLength)
            serialPatternLength = serialLen();
            
         return serialPatternLength;
      }
      
      size_t getNumStripeTargetIDs() const
      {
         return getStripeTargetIDs()->size();
      }

      /**
       * Get index of target in stripe vector for given file offset.
       */
      virtual size_t getStripeTargetIndex(int64_t pos) const = 0;

      /**
       * Get targetID for given file offset.
       */
      virtual uint16_t getStripeTargetID(int64_t pos) const = 0;
      
      /**
       * Note: this is called quite often, so we have a const version to enable better compiler
       * code optimizations. (there are some special cases where targetIDs need to be modified, so
       * we also have the non-const/modifyable version.)
       */
      virtual const UInt16Vector* getStripeTargetIDs() const = 0;

      /**
       * Get copy of stripe target IDs.
       */
      virtual void getStripeTargetIDs(UInt16Vector* outTargetIDs) const = 0;

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

      /**
       * Map stringID to numeric ID
       *
       * only implemented by raid0 pattern
       */
      virtual bool mapStringIDs(StringUnsignedMap* idMap)
      {
         return false;
      }

};

#endif /*STRIPEPATTERN_H_*/
