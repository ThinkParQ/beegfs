#pragma once

#include <assert.h>
#include <stdint.h>

#include <type_traits>

#include "ArrayIteration.h"

// This class provides functionality similar to std::span, with the difference that
//  - it is available to use (std::span is C++20; at the time of writing we're on C++17).
//  - it is probably somewhat simpler but less featureful
//  - it integrates better with our other helper classes, like (untyped) Slice (see ArrayTypeTraits.h).
//
//  We can consider removing it when we move to C++20, but there may also be
//  reasons to keep it.

template<typename T>
class ArraySlice
{
   T *mData = 0;
   size_t mCount = 0;
public:
   T *data() const
   {
      return mData;
   }
   size_t count() const
   {
      return mCount;
   }
   T& operator[](size_t i)
   {
      assert(i < mCount);
      return mData[i];
   }

   IterateAsValues<T> iterateAsValues() const
   {
      return IterateAsValues<T>(mData, mCount);
   }
   IterateAsPointers<T> iterateAsPointers() const
   {
      return IterateAsPointers<const T>(mData, mCount);
   }
   IterateAsPointers<const T> iterateAsConstPointers() const
   {
      return IterateAsPointers<const T>(mData, mCount);
   }
   IterateAsRefs<T> iterateAsRefs() const
   {
      return IterateAsRefs<T>(mData, mCount);
   }
   IterateAsRefs<const T> iterateAsConstRefs() const
   {
      return IterateAsRefs<const T>(mData, mCount);
   }

   ArraySlice() = default;

   ArraySlice(T *data, size_t count) : mData(data), mCount(count) {}

   template<size_t N> // ref-hack to accept plain C arrays
   ArraySlice(T (&ref)[N]) : mData(&ref[0]), mCount(N) {}
};
