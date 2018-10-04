#ifndef COMMON_NUMERICID_H
#define COMMON_NUMERICID_H

#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/StringTk.h>

#include <ios>
#include <sstream>


/*
 * template class to represent a Numeric ID; which can be used as Numeric Node ID or Group ID, etc.
 *
 * we use classes instead of a simple typedef to achieve "type" safety with nodeIDs, groupIDs,
 * targetIDs, etc.
 *
 * implemented as template because different kinds of IDs might have different base data types
 */
template <typename T, typename Tag>
class NumericID final
{
   public:
      typedef T ValueT;

      NumericID():
         value(0) { }

      explicit NumericID(T value):
         value(value) { }

      T val() const
      {
         return value;
      }

      std::string str() const
      {
         std::stringstream out;
         out << value;
         return out.str();
      }

      std::string strHex() const
      {
         std::stringstream out;
         out << std::hex << std::uppercase << value;
         return out.str();
      }

      void fromHexStr(const std::string& valueStr)
      {
         std::stringstream in(valueStr);
         in >> std::hex >> value;
      }

      void fromStr(const std::string& valueStr)
      {
         std::stringstream in(valueStr);
         in >> value;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->value;
      }

      bool operator==(const NumericID& other) const
      {
         return (value == other.value);
      }

      bool operator!=(const NumericID& other) const
      {
         return (value != other.value);
      }

      bool operator<(const NumericID& other) const
      {
         return (value < other.value);
      }

      bool operator>(const NumericID& other) const
      {
         return (value > other.value);
      }

      bool operator<=(const NumericID& other) const
      {
         return (value <= other.value);
      }

      bool operator>=(const NumericID& other) const
      {
         return (value >= other.value);
      }

      NumericID& operator++()
      {
         ++value;
         return *this;
      }

      NumericID operator++(int)
      {
         NumericID result = *this;
         ++value;
         return result;
      }

      NumericID& operator--()
      {
         --value;
         return *this;
      }

      NumericID operator--(int)
      {
         NumericID result = *this;
         --value;
         return result;
      }

      bool operator!() const
      {
         return (value == 0);
      }

      explicit operator bool() const
      {
         return value != 0;
      }

      friend std::ostream& operator<< (std::ostream& out, const NumericID& obj)
      {
         return out << obj.str();
      }

      friend std::istream& operator>> (std::istream& in, NumericID& obj)
      {
         in >> obj.value;
         return in;
      }

   protected:
      T value;
};

namespace std
{
   template<typename T, typename Tag>
   struct hash<NumericID<T, Tag>>
   {
      size_t operator()(const NumericID<T, Tag>& id) const
      {
         return std::hash<T>()(id.val());
      }
   };

   template<typename T, typename Tag>
   class numeric_limits<NumericID<T, Tag>>
   {
      public:
         static constexpr bool is_specialized = true;
         static constexpr bool is_signed = std::numeric_limits<T>::is_signed;
         static constexpr bool is_integer = std::numeric_limits<T>::is_integer;
         static constexpr bool is_exact = std::numeric_limits<T>::is_exact;
         static constexpr bool has_infinity = std::numeric_limits<T>::has_infinity;
         static constexpr bool has_quiet_NaN = std::numeric_limits<T>::has_quiet_NaN;
         static constexpr bool has_signaling_NaN = std::numeric_limits<T>::has_signaling_NaN;
         static constexpr bool has_denorm = std::numeric_limits<T>::has_denorm;
         static constexpr bool has_denorm_loss = std::numeric_limits<T>::has_denorm_loss;
         static constexpr std::float_round_style round_style = std::numeric_limits<T>::round_style;
         static constexpr bool is_iec559 = std::numeric_limits<T>::is_iec559;
         static constexpr bool is_bounded = std::numeric_limits<T>::is_bounded;
         static constexpr bool is_modulo = std::numeric_limits<T>::is_modulo;
         static constexpr int digits = std::numeric_limits<T>::digits;
         static constexpr int digits10 = std::numeric_limits<T>::digits10;
         static constexpr int max_digits10 = std::numeric_limits<T>::max_digits10;
         static constexpr int radix = std::numeric_limits<T>::radix;
         static constexpr int min_exponent = std::numeric_limits<T>::min_exponent;
         static constexpr int min_exponent10 = std::numeric_limits<T>::min_exponent10;
         static constexpr int max_exponent = std::numeric_limits<T>::max_exponent;
         static constexpr int max_exponent10 = std::numeric_limits<T>::max_exponent10;
         static constexpr bool traps = std::numeric_limits<T>::traps;
         static constexpr bool tinyness_before = std::numeric_limits<T>::tinyness_before;

         static NumericID<T, Tag> min() noexcept { return NumericID<T, Tag>(numeric_limits<T>::min()); }
         static NumericID<T, Tag> lowest() noexcept { return NumericID<T, Tag>(numeric_limits<T>::lowest()); }
         static NumericID<T, Tag> max() noexcept { return NumericID<T, Tag>(numeric_limits<T>::max()); }
         static NumericID<T, Tag> epsilon() noexcept { return NumericID<T, Tag>(numeric_limits<T>::epsilon()); }
         static NumericID<T, Tag> round_error() noexcept { return NumericID<T, Tag>(numeric_limits<T>::round_error()); }
         static NumericID<T, Tag> infinity() noexcept { return NumericID<T, Tag>(numeric_limits<T>::infinity()); }
         static NumericID<T, Tag> quiet_NaN() noexcept { return NumericID<T, Tag>(numeric_limits<T>::quiet_NaN()); }
         static NumericID<T, Tag> signaling_NaN() noexcept { return NumericID<T, Tag>(numeric_limits<T>::signaling_NaN()); }
         static NumericID<T, Tag> denorm_min() noexcept { return NumericID<T, Tag>(numeric_limits<T>::denorm_min()); }
   };
}
#endif /*COMMON_NUMERICID_H*/
