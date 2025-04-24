#pragma once

// Helper classes for ArraySlice template class.
// This file can probably be removed when ArraySlice.h gets removed.

#include <stddef.h> // size_t

namespace
{

// Pointer-based iterator that dereferences to a value copy.
// Needed to implement IterateAsValues.
template<typename T>
class ValueIter
{
   T *mPtr = nullptr;
public:
   T operator*() const { return *mPtr; }
   ValueIter& operator++() { mPtr++; return *this; }
   ValueIter& operator--() { mPtr--; return *this; }
   ValueIter operator++(int) { ValueIter old = *this; mPtr++; return old; }
   ValueIter operator--(int) { ValueIter old = *this; mPtr--; return old; }
   bool operator==(ValueIter other) { return mPtr == other.mPtr; }
   bool operator!=(ValueIter other) { return mPtr != other.mPtr; }
   bool operator<(ValueIter other) { return mPtr < other.mPtr; }
   bool operator>(ValueIter other) { return mPtr > other.mPtr; }
   bool operator<=(ValueIter other) { return mPtr <= other.mPtr; }
   bool operator>=(ValueIter other) { return mPtr >= other.mPtr; }
   ValueIter() = default;
   ValueIter(T *ptr) : mPtr(ptr) {}
};

// Wrapper over pointer that dereferences to itself.
// Needed to implement IterateAsPointers.
template<typename T>
class PointerIter
{
   T *mPtr = nullptr;
public:
   T *operator*() const { return mPtr; }
   PointerIter& operator++() { mPtr++; return *this; }
   PointerIter& operator--() { mPtr--; return *this; }
   PointerIter operator++(int) { PointerIter old = *this; mPtr++; return old; }
   PointerIter operator--(int) { PointerIter old = *this; mPtr--; return old; }
   bool operator==(PointerIter other) { return mPtr == other.mPtr; }
   bool operator!=(PointerIter other) { return mPtr != other.mPtr; }
   bool operator<(PointerIter other) { return mPtr < other.mPtr; }
   bool operator>(PointerIter other) { return mPtr > other.mPtr; }
   bool operator<=(PointerIter other) { return mPtr <= other.mPtr; }
   bool operator>=(PointerIter other) { return mPtr >= other.mPtr; }
   PointerIter() = default;
   PointerIter(T *ptr) : mPtr(ptr) {}
};

// Simple pointer-delineated range. Iteration by value (every element gets copied).
// Iterate like:
//   for (T t : IterateAsValues<T>(...)) {...}
template<typename T>
class IterateAsValues
{
   T *mBegin = 0;
   T *mEnd = 0;
public:
   ValueIter<T> begin() const { return ValueIter(mBegin); }
   ValueIter<T> end() const { return ValueIter(mEnd); }
   IterateAsValues() = default;
   IterateAsValues(T *data, size_t count) : mBegin(data), mEnd(data + count) {}
};

// Simple pointer-delineated range. Iteration as pointers to the elements.
// Iterate like:
//   for (T *ptr : IterateAsPointers<T>(...)) {...}
template<typename T>
class IterateAsPointers
{
   T *mBegin = 0;
   T *mEnd = 0;
public:
   PointerIter<T> begin() const { return PointerIter(mBegin); }
   PointerIter<T> end() const { return PointerIter(mEnd); }
   IterateAsPointers() = default;
   IterateAsPointers(T *data, size_t count) : mBegin(data), mEnd(data + count) {}
};

// Simple pointer-delineated range. Iteration as references to the elements.
// Iterate like:
//   for (T& ptr : IterateAsRefs<T>(...)) {...}
template<typename T>
class IterateAsRefs
{
   T *mBegin = 0;
   T *mEnd = 0;
public:
   T *begin() const { return mBegin; }
   T *end() const { return mEnd; }
   IterateAsRefs() = default;
   IterateAsRefs(T *data, size_t count) : mBegin(data), mEnd(data + count) {}
};

}
