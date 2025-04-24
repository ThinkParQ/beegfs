#pragma once

#include <common/memory/Slice.h>

#include "ArraySlice.h"

#include <type_traits>

namespace
{
   template<typename T>
   class Util
   {
   public:
      // Basic constraints that I think should be required when converting to
      // RO_Slice, WO_Slice, and Slice.
      // Note that these slices are type-agnostic and we copy using std::memcpy().
      static constexpr bool ro_slice_possible = std::is_trivial<T>::value && ! std::is_volatile<T>::value;
      static constexpr bool wo_slice_possible = std::is_trivial<T>::value && ! std::is_volatile<T>::value
         && ! std::is_const<T>::value;
   };

   template<typename T, size_t N> // array-ref hack to get size automatically
   static ArraySlice<T> As_ArraySlice(T (&ref)[N])
   {
      return ArraySlice(&ref[0], N);
   }

   template<typename T, size_t N> // array-ref hack to get size automatically
   static RO_Slice As_RO_Slice(T (&ref)[N])
   {
      static_assert(Util<T>::ro_slice_possible, "type must be trivial");
      return RO_Slice(&ref[0], N * sizeof (T));
   }

   template<typename T, size_t N> // array-ref hack to get size automatically
   static WO_Slice As_WO_Slice(T (&ref)[N])
   {
      static_assert(Util<T>::wo_slice_possible, "type must be trivial and writeable");
      return WO_Slice(&ref[0], N * sizeof (T));
   }

   template<typename T, size_t N> // array-ref hack to get size automatically
   static Slice As_Slice(T (&ref)[N])
   {
      static_assert(Util<T>::wo_slice_possible, "type must be trivial and writeable");
      return Slice(&ref[0], N * sizeof (T));
   }

   template<typename T>
   static RO_Slice As_RO_Slice(ArraySlice<T> s)
   {
      static_assert(Util<T>::ro_slice_possible, "type must be trivial");
      return RO_Slice(s.data(), s.count() * sizeof (T));
   }

   template<typename T>
   static WO_Slice As_WO_Slice(ArraySlice<T> s)
   {
      static_assert(Util<T>::wo_slice_possible, "type must be trivial and writeable");
      return WO_Slice(s.data(), s.count() * sizeof (T));
   }

   template<typename T>
   static Slice As_Slice(ArraySlice<T> s)
   {
      static_assert(Util<T>::ro_slice_possible, "type must be trivial and writeable");
      return Slice(s.data(), s.count() * sizeof (T));
   }
};
