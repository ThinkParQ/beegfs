#pragma once

#include "Slice.h"

#include <cstdlib>
#include <utility>

namespace
{

// A simple (maybe overly simple) class that owns a heap-allocated buffer.
// There is no size (only capacity), no contained type, no pluggable allocator.
// Also no exceptions.
// NOTE: allocation happens in the reset() function. You must check the return value.
class MallocBuffer
{
   void *mData = 0;
   size_t mCapacity = 0;

public:

   void *data() const
   {
      return mData;
   }

   size_t capacity() const
   {
      return mCapacity;
   }

   void drop()
   {
      std::free(mData);
      mData = 0;
      mCapacity = 0;
   }

   [[nodiscard]] bool reset(size_t capacity)
   {
      drop();
      mData = std::malloc(capacity);
      if (! mData)
         return false;
      mCapacity = capacity;
      return true;
   }

   Slice as_slice() const
   {
      return Slice(mData, mCapacity);
   }

   // Avoid accidental copies
   MallocBuffer(MallocBuffer const& other) = delete;
   MallocBuffer& operator=(MallocBuffer const& other) = delete;

   MallocBuffer& operator=(MallocBuffer&& other)
   {
      drop();
      std::swap(mData, other.mData);
      std::swap(mCapacity, other.mCapacity);
      return *this;
   }

   MallocBuffer(MallocBuffer&& other)
   {
      std::swap(mData, other.mData);
      std::swap(mCapacity, other.mCapacity);
   }

   MallocBuffer() = default;

   ~MallocBuffer()
   {
      drop();
   }
};

}
