#ifndef STRIPEPATTERN_H_
#define STRIPEPATTERN_H_

/**
 * Note: Do not instantiate this "class" directly (it contains pure virtual functions)
 */

#include <common/Common.h>
#include <common/storage/StoragePoolId.h>
#include <common/toolkit/SerializationTypes.h>
#include <common/toolkit/vector/UInt16Vec.h>


// pattern types
#define STRIPEPATTERN_Invalid      0
#define STRIPEPATTERN_Raid0        1
#define STRIPEPATTERN_Raid10       2
#define STRIPEPATTERN_BuddyMirror  3

// minimum allowed stripe pattern chunk size (in bytes)
#define STRIPEPATTERN_MIN_CHUNKSIZE    (1024*64)

// pattern serialization defs
#define STRIPEPATTERN_HEADER_LENGTH \
   (sizeof(unsigned) + sizeof(unsigned) + sizeof(unsigned))
/* length + type + chunkSize*/


struct StripePatternHeader
{
   // everything in this struct is in host byte order!

   unsigned       patternLength; // in bytes
   unsigned       patternType; // the type of pattern, defined as STRIPEPATTERN_x
   unsigned       chunkSize;
   // storagePoolId is unused in the client at the moment; however we deserialize it to avoid human
   // errors later
   StoragePoolId  storagePoolId;
};


struct StripePattern;
typedef struct StripePattern StripePattern;


static inline void StripePattern_initFromPatternType(StripePattern* this,
   unsigned patternType, unsigned chunkSize);

extern void StripePattern_virtualDestruct(struct StripePattern* this);

extern bool StripePattern_deserializePatternPreprocess(DeserializeCtx* ctx,
   const char** outPatternStart, uint32_t* outPatternLength);
extern bool __StripePattern_deserializeHeader(DeserializeCtx* ctx,
   struct StripePatternHeader* outPatternHeader);

// static functions
extern StripePattern* StripePattern_createFromBuf(const char* patternStart,
   uint32_t patternLength);

// virtual functions
extern UInt16Vec* StripePattern_getMirrorTargetIDs(StripePattern* this);

// getters & setters
static inline int StripePattern_getPatternType(StripePattern* this);
static inline unsigned StripePattern_getChunkSize(StripePattern* this);
static inline int64_t StripePattern_getChunkStart(StripePattern* this, int64_t pos);
static inline int64_t StripePattern_getNextChunkStart(StripePattern* this, int64_t pos);
static inline int64_t StripePattern_getChunkEnd(StripePattern* this, int64_t pos);


struct StripePattern
{
   unsigned patternType; // STRIPEPATTERN_...
   unsigned chunkSize; // must be a power of two (optimizations rely on it)

   unsigned serialPatternLength; // for (de)serialization

   // virtual functions
   void (*uninit) (StripePattern* this);

   // (de)serialization
   bool (*deserializePattern) (StripePattern* this, DeserializeCtx* ctx);

   size_t (*getStripeTargetIndex) (StripePattern* this, int64_t pos);
   uint16_t (*getStripeTargetID) (StripePattern* this, int64_t pos);
   void (*getStripeTargetIDsCopy) (StripePattern* this, UInt16Vec* outTargetIDs);
   UInt16Vec* (*getStripeTargetIDs) (StripePattern* this);
   UInt16Vec* (*getMirrorTargetIDs) (StripePattern* this);
   unsigned (*getMinNumTargets) (StripePattern* this);
   unsigned (*getDefaultNumTargets) (StripePattern* this);
};


void StripePattern_initFromPatternType(StripePattern* this, unsigned patternType,
   unsigned chunkSize)
{
   this->patternType = patternType;
   this->chunkSize = chunkSize;

   this->serialPatternLength = 0;

   // pre-defined virtual methods
   this->getMirrorTargetIDs = StripePattern_getMirrorTargetIDs;
}


int StripePattern_getPatternType(StripePattern* this)
{
   return this->patternType;
}

unsigned StripePattern_getChunkSize(StripePattern* this)
{
   return this->chunkSize;
}

int64_t StripePattern_getChunkStart(StripePattern* this, int64_t pos)
{
   // the code below is an optimization (wrt division) for the following line:
   //    int64_t chunkStart = pos - (pos % this->chunkSize);

   // "& chunkSize -1" instead of "%", because chunkSize is a power of two
   unsigned posModChunkSize = pos & (this->chunkSize - 1);

   int64_t chunkStart = pos - posModChunkSize;

   return chunkStart;
}

/**
 * Get the exact file position where the next chunk starts
 */
int64_t StripePattern_getNextChunkStart(StripePattern* this, int64_t pos)
{
   return StripePattern_getChunkStart(this, pos) + this->chunkSize;
}

/**
 * Get the exact file position where the current chunk ends
 */
int64_t StripePattern_getChunkEnd(StripePattern* this, int64_t pos)
{
   return StripePattern_getNextChunkStart(this, pos) - 1;
}

#endif /*STRIPEPATTERN_H_*/
