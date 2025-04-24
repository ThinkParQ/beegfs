#pragma once

#include <string.h>

#include "Slice.h"

namespace
{

// String slice type, not necessarily zero-terminated. It's basically like
// Slice but it comes with the expectation to contain ASCII/UTF-8 test.  It is
// char-typed and has just a tiny bit of helper functionality attached
// (equals/startswith/endswith).  It converts easily to Slice using slice()
// method.

class String
{
   const char *mData = nullptr;
   size_t mSizeBytes = 0;
public:
   const char *data() const
   {
      return mData;
   }
   size_t sizeBytes() const
   {
      return mSizeBytes;
   }
   String offsetBytes(size_t offsetBytes) const
   {
      if (offsetBytes > mSizeBytes)
         offsetBytes = mSizeBytes;
      return String(mData + offsetBytes, mSizeBytes - offsetBytes);
   }
   String limitBytes(size_t sizeBytes) const
   {
      if (sizeBytes >= mSizeBytes)
         sizeBytes = mSizeBytes;
      return String(mData, sizeBytes);
   }
   bool equals(String other) const
   {
      if (mSizeBytes != other.mSizeBytes)
         return false;
      return !memcmp(mData, other.mData, mSizeBytes);
   }
   bool startswith(String other) const
   {
      if (mSizeBytes < other.mSizeBytes)
         return false;
      return limitBytes(other.mSizeBytes).equals(other);
   }
   bool endswith(String other) const
   {
      if (mSizeBytes < other.mSizeBytes)
         return false;
      return offsetBytes(mSizeBytes - other.mSizeBytes).equals(other);
   }
   RO_Slice slice() const
   {
      return RO_Slice(mData, mSizeBytes);
   };
   operator RO_Slice() const
   {
      return RO_Slice(mData, mSizeBytes);
   }
   String() = default;
   String(const char *s, size_t sizeBytes)
   {
      mData = s;
      mSizeBytes = sizeBytes;
   };
};

// A zero-terminated string slice type.
// It is non-allocating and non-owning, storing pointer + length pair internally.
// The contract is that it must be a C-string of the given size i.e. it's
// sizeBytes()th byte must be zero.

class StringZ
{
   const char *mData = "";
   size_t mSizeBytes = 0;  // number of bytes, excluding the terminating NUL.
public:
   const char *data() const
   {
      return mData;
   }
   const char *c_str() const
   {
      return mData;
   }
   size_t sizeBytes() const
   {
      return mSizeBytes;
   }
   size_t sizeBytesZ() const  // size including terminating zero
   {
      return mSizeBytes + 1;
   }
   StringZ offsetBytes(size_t offsetBytes) const
   {
      if (offsetBytes > mSizeBytes)
         offsetBytes = mSizeBytes;
      return StringZ::fromZeroTerminated(mData + offsetBytes, mSizeBytes - offsetBytes);
   }
   // NOTE: the result is not zero-terminated, so we can only return String!
   String limitBytes(size_t sizeBytes) const
   {
      if (sizeBytes >= mSizeBytes)
         sizeBytes = mSizeBytes;
      return String(mData, sizeBytes);
   }
   RO_Slice slice() const
   {
      return RO_Slice(mData, mSizeBytes);
   }
   RO_Slice sliceZ() const
   {
      return RO_Slice(mData, mSizeBytes + 1);
   }
   String string() const
   {
      return String(mData, mSizeBytes);
   }
   operator String() const
   {
      return String(mData, mSizeBytes);
   }
   bool equals(StringZ other) const
   {
      return string().equals(other.string());
   }
   bool startswith(StringZ other) const
   {
      return string().startswith(other.string());
   }
   bool endswith(StringZ other) const
   {
      return string().endswith(other.string());
   }
   StringZ() = default;
   explicit StringZ(const char *data)
   {
      mData = data;
      mSizeBytes = strlen(data);
   }
   static StringZ fromZeroTerminated(const char *data, size_t sizeBytes)
   {
      StringZ out;
      out.mData = data;
      out.mSizeBytes = sizeBytes;
      return out;
   }
};

// We can produce String and StringZ literals using "Hello"_s and "Hello"_sz syntax.

inline String operator ""_s(const char *lit, size_t size)
{
   return String(lit, size);
}

inline StringZ operator ""_sz(const char *lit, size_t size)
{
   return StringZ::fromZeroTerminated(lit, size);
}

}
