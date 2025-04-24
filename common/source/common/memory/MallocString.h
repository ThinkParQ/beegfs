#pragma once

#include "String.h"

namespace
{

// Similar to MallocBuffer, but for strings.
// We tolerate a bit of boilerplate and duplication in exchange for API simplicity.
// The implementation currently returns a zero-sized string as a pointer to a
// statically allocated 0 byte (a "" literal). We still store an NULL pointer
// internally and check the case on .get(). This may not seem ideal but it
// simplifies the code. There might be a better solution.
class MallocString
{
   char *mData = 0;
   size_t mSizeBytes = 0;

public:
   const char *data() const
   {
      return mData ? mData : "";
   }

   size_t sizeBytes() const
   {
      return mSizeBytes;
   }

   size_t sizeBytes0() const
   {
      return mSizeBytes + 1;
   }

   operator String() const
   {
      return String(mData, mSizeBytes);
   }

   operator StringZ() const
   {
      return StringZ::fromZeroTerminated(data(), mSizeBytes);
   }

   void drop()
   {
      std::free(mData);
      mData = 0;
      mSizeBytes = 0;
   }

   [[nodiscard]] bool reset(const char *data, size_t sizeBytes)
   {
      drop();
      mData = (char *) std::malloc(sizeBytes + 1);
      if (! mData)
         return false;
      mSizeBytes = sizeBytes;
      memcpy(mData, data, sizeBytes);
      mData[sizeBytes] = 0;
      return true;
   }

   [[nodiscard]] bool reset(String string)
   {
      return reset(string.data(), string.sizeBytes());
   }

   [[nodiscard]] bool reset(StringZ string)
   {
      return reset(string.data(), string.sizeBytes());
   }

   // Avoid accidental copies
   MallocString(MallocString const& other) = delete;
   MallocString& operator=(MallocString const& other) = delete;

   MallocString& operator=(MallocString&& other)
   {
      drop();
      std::swap(mData, other.mData);
      std::swap(mSizeBytes, other.mSizeBytes);
      return *this;
   }

   MallocString(MallocString&& other)
   {
      std::swap(mData, other.mData);
      std::swap(mSizeBytes, other.mSizeBytes);
   }

   MallocString() = default;

   ~MallocString()
   {
      drop();
   }
};

}
