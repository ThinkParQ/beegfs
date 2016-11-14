#ifndef MINMAXSTORE_H_
#define MINMAXSTORE_H_

#include <limits>

/**
 * Class to determine the minimum and maximum value of a set.
 */
template <typename T>
class MinMaxStore {
   public:
      /**
       * Default constructor: Sets minimum to std::numric_limits<T>::max() and maximum to
       * std::numeric_limits<T>::min().
       */
      MinMaxStore() : min(std::numeric_limits<T>::max() ), max(std::numeric_limits<T>::min() )
      { }

      /**
       * Constructor to initizlize the minimum and maximum to the same value.
       */
      MinMaxStore(T value) : min(value), max(value)
      { }

      /**
       * Constructor to initialize the minimum and maximum to two different values.
       * Note: max has to be greater or equali to min.
       */
      MinMaxStore(T min, T max) : min(min), max(max)
      { }


   private:
      T min;
      T max;


   public:
      /**
       * Returns the minimum of all numbers entered so far.
       */
      T getMin() const
      {
         return this->min;
      }

      /**
       * Returns the maximum of all numbers entered so far.
       */
      T getMax() const
      {
         return this->max;
      }

      /**
       * Enters a new number and updates internal minimum and maximum variables.
       */
      void enter(const T value)
      {
         this->min = BEEGFS_MIN(this->min, value);
         this->max = BEEGFS_MAX(this->max, value);
      }

};

#endif // MINMAXSTORE_H_
