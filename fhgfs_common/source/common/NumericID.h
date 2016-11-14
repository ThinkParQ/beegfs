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
template <typename T>
class NumericID
{
   public:
      typedef void (NumericID::*bool_type)() const;

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

      bool operator==(const NumericID<T>& other) const
      {
         return (value == other.value);
      }

      bool operator!=(const NumericID<T>& other) const
      {
         return (value != other.value);
      }

      bool operator<(const NumericID<T>& other) const
      {
         return (value < other.value);
      }

      bool operator>(const NumericID<T>& other) const
      {
         return (value > other.value);
      }

      bool operator<=(const NumericID<T>& other) const
      {
         return (value <= other.value);
      }

      bool operator>=(const NumericID<T>& other) const
      {
         return (value >= other.value);
      }

      NumericID<T>& operator++()
      {
         ++value;
         return *this;
      }

      NumericID<T> operator++(int)
      {
         NumericID<T> result = *this;
         ++value;
         return result;
      }

      NumericID<T>& operator--()
      {
         --value;
         return *this;
      }

      NumericID<T> operator--(int)
      {
         NumericID<T> result = *this;
         --value;
         return result;
      }

      bool operator!() const
      {
         return (value == 0);
      }

      operator bool_type() const
      {
         return value ? &NumericID::bool_value_fn : 0;
      }

      friend std::ostream& operator<< (std::ostream& out, const NumericID<T>& obj)
      {
         return out << obj.str();
      }

   protected:
      T value;

   private:
      void bool_value_fn() const {}
};

namespace std
{
   // specializing std::numeric_limits for NumericID
   template<typename T>
   class numeric_limits<NumericID<T>>
   {
      public:
         static int min()
         {
            return std::numeric_limits<T>::min();
         };

         static int max()
         {
            return std::numeric_limits<T>::max();
         };
   };
}

#endif /*COMMON_NUMERICID_H*/
