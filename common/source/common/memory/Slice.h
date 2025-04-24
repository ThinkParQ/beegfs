#pragma once

#include <assert.h>
#include <string.h>

// Basic generic memory region handling.
// There are also read-only and write-only variants.
// Added type safety because of automatic bounds checks and convenient copy functions.

namespace
{

class Slice;

class RO_Slice
{
   const void *mData = nullptr;
   size_t mSize = 0;
public:
   const void *data() const
   {
      return mData;
   }
   size_t sizeBytes() const
   {
      return mSize;
   }
   RO_Slice offsetBytes(size_t offBytes) const
   {
      assert(offBytes <= mSize);
      return RO_Slice((char *) mData + offBytes, mSize - offBytes);
   }
   RO_Slice limitBytes(size_t sizeBytes) const
   {
      assert(sizeBytes <= mSize);
      return RO_Slice(mData, mSize - sizeBytes);
   }
   RO_Slice() = default;
   RO_Slice(const void *data, size_t sizeBytes)
   {
      mData = data;
      mSize = sizeBytes;
   }
};

class WO_Slice
{
   void *mData = nullptr;
   size_t mSize = 0;
public:
   // even though this is WO, we still return the data -- it would be unnecessarily unergonomic to not offer it back.
   // Nevertheless the WO_Slice is less capable than a full Slice -- we can't easily make an RO_Slice out of it for example.
   void *data() const
   {
      return mData;
   }
   size_t sizeBytes() const
   {
      return mSize;
   }
   WO_Slice offsetBytes(size_t offBytes) const
   {
      assert(offBytes <= mSize);
      return WO_Slice((char *) mData + offBytes, mSize - offBytes);
   }
   WO_Slice limitBytes(size_t sizeBytes) const
   {
      assert(sizeBytes <= mSize);
      return WO_Slice(mData, mSize - sizeBytes);
   }
   WO_Slice() = default;
   WO_Slice(void *data, size_t sizeBytes)
   {
      mData = data;
      mSize = sizeBytes;
   }
};

class Slice
{
   void *mData = nullptr;
   size_t mSize = 0;
public:
   void *data() const
   {
      return mData;
   }
   size_t sizeBytes() const
   {
      return mSize;
   }
   Slice offsetBytes(size_t offBytes) const
   {
      assert(offBytes <= mSize);
      return Slice((char *) mData + offBytes, mSize - offBytes);
   }
   Slice limitBytes(size_t sizeBytes) const
   {
      assert(sizeBytes <= mSize);
      return Slice(mData, mSize - sizeBytes);
   }
   operator WO_Slice() const
   {
      return WO_Slice(mData, mSize);
   }
   operator RO_Slice() const
   {
      return RO_Slice(mData, mSize);
   }
   Slice() = default;
   Slice(void *data, size_t sizeBytes)
   {
      mData = data;
      mSize = sizeBytes;
   }
};


static inline void sliceFill0(WO_Slice dest)
{
   if (dest.sizeBytes() > 0)  // passing NULL to memset() leads to UB according to standard.
      memset(dest.data(), 0, dest.sizeBytes());
}

// A copy function that copies all the source slice. Source slice must be <= destination slice.
// This will probably the most commonly used function.
static inline void sliceCopy(WO_Slice dest, RO_Slice source)
{
   assert(source.sizeBytes() <= dest.sizeBytes());
   memcpy(dest.data(), source.data(), source.sizeBytes());
}

// A variant that copies as much as possible from input to output.
static inline void sliceCopyFill0(WO_Slice dest, RO_Slice source)
{
   sliceCopy(dest, source);
   sliceFill0(dest.offsetBytes(source.sizeBytes()));
}

}
