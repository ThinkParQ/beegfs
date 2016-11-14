#ifndef STRIPEPATTERN_H_
#define STRIPEPATTERN_H_

#include <Common.h>

// pattern types
#define STRIPEPATTERN_Invalid 0
#define STRIPEPATTERN_Raid0   1

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
      virtual ~StripePattern()
      {
      }


      static StripePattern* createFromBuf(const char* patternStart,
         struct StripePatternHeader& patternHeader);

      
      /**
       * Creates a clone of the pattern.
       * Note: The clone must be deleted by the caller.
       */ 
      virtual StripePattern* clone() const = 0;

      /**
       * Clones the pattern and applies new targetIDs to the clone.
       * Note: The clone must be deleted by the caller.
       */
      virtual StripePattern* clone(const StringVector& targetIDs) const = 0;
      

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
         this->chunkSize = chunkSize ? chunkSize : 256*1024;
         
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
      unsigned patternType;
      unsigned chunkSize;
      
      // (de)serialization
      unsigned serialPatternLength;
      
      static bool deserializeHeader(const char* buf, size_t bufLen,
         struct StripePatternHeader* outPatternHeader);

      void serializeHeader(char* buf);
      
      
   public:
      // getters & setters
      int getPatternType() const
      {
         return patternType;
      }
      
      unsigned getChunkSize() const
      {
         return chunkSize;
      }
      
      int64_t getChunkStart(int64_t pos) const
      {
         int64_t chunkStart = pos - (pos % chunkSize); 
         
         return chunkStart;
      }

      int64_t getNextChunkStart(int64_t pos) const
      {
         int64_t nextChunkStart = pos + (chunkSize - (pos % chunkSize) ); 
         
         return nextChunkStart; 
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
      
      virtual size_t getStripeTargetIndex(int64_t pos) const = 0;

      virtual std::string getStripeTargetID(int64_t pos) const = 0;
      
      virtual unsigned getMinNumTargets() const = 0;
      
      virtual unsigned getDefaultNumTargets() const = 0;
      
      virtual void getStripeTargetIDs(StringVector* outTargetIDs) const = 0;
      
      virtual StringVector* getStripeTargetIDs() = 0;
      
      
};

#endif /*STRIPEPATTERN_H_*/
