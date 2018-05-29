#ifndef ZIP_ITERATOR_H_
#define ZIP_ITERATOR_H_

#include <iterator>

/**
 * Triple helper struct to be used as value_type of ZipIterator with three elements.
 */
template<class F, class S, class T>
struct triple
{
   typedef F first_type;
   typedef S second_type;
   typedef T third_type;

   F first;
   S second;
   T third;

   triple()
   { }

   triple(F first, S second, T third)
      : first(first), second(second), third(third)
   { }

   triple& operator=(const triple& other)
   {
      first = other.first;
      second = other.second;
      third = other.third;

      return *this;
   }

   bool operator!=(const triple<F, S, T>& rhs)
   {
      return !(*this == rhs);
   }

   bool operator<(const triple<F, S, T>& rhs)
   {
      if (this->first < rhs.first)
         return true;
      else
      if (rhs.first < this->first)
         return false;
      else
      if (this->second < rhs.second)
         return true;
      else
      if (rhs.second < this->second)
         return false;
      else
      if (this->third < rhs.third)
         return true;
      else
         return false;
   }

   bool operator<=(const triple<F, S, T>& rhs)
   {
      return (*this == rhs) || (*this < rhs);
   }

   bool operator>(const triple<F, S, T>& rhs)
   {
      return !(*this <= rhs);
   }

   bool operator>=(const triple<F, S, T>& rhs)
   {
      return !(*this < rhs);
   }
};

/**
 * 3-valued zip iterator.
 * Holds a triple of pointers to value_type of the contained iterators.
 */
template<typename F, typename S, typename T=void>
class ZipIterator
{
   public:
      // Public typedefs.
      typedef std::ptrdiff_t difference_type;
      typedef triple<typename F::pointer, typename S::pointer, typename T::pointer>
         value_type;
      typedef value_type* pointer;
      typedef value_type& reference;
      typedef std::input_iterator_tag iterator_category;

      ZipIterator(F first_iter, S second_iter, T third_iter)
         : first_iter(first_iter),
           second_iter(second_iter),
           third_iter(third_iter)
      { }

   private:
      F first_iter;
      S second_iter;
      T third_iter;

      // The most recently constructed value triple.
      value_type value;

   public:
      ZipIterator& operator++()
      {
         ++first_iter;
         ++second_iter;
         ++third_iter;

         return *this;
      }

      bool operator==(const ZipIterator& other) const
      {
         return first_iter == other.first_iter
             && second_iter == other.second_iter
             && third_iter == other.third_iter;
      }

      bool operator!=(const ZipIterator& other) const
      {
         return !(*this == other);
      }

      reference operator*()
      {
         value = value_type(&*first_iter, &*second_iter, &*third_iter);
         return value;
      }

      pointer operator->()
      {
         return &operator*();
      }
};

/**
 * 2-valued zip itarator.
 * Holds a pair of pointers to value_types of the contained iterators.
 */
template<typename F, typename S>
class ZipIterator<F, S, void>
{
   public:
      // Public typedefs.
      typedef std::ptrdiff_t difference_type;
      typedef std::pair<typename F::pointer, typename S::pointer> value_type;
      typedef value_type* pointer;
      typedef value_type& reference;
      typedef std::input_iterator_tag iterator_category;

      ZipIterator(F first_iter, S second_iter)
         : first_iter(first_iter), second_iter(second_iter)
      { }

   private:
      F first_iter;
      S second_iter;

      // Most recently constructed value pair.
      value_type value;

   public:
      ZipIterator& operator++()
      {
         ++first_iter;
         ++second_iter;

         return *this;
      }

      bool operator==(const ZipIterator& other) const
      {
         return first_iter == other.first_iter && second_iter == other.second_iter;
      }

      bool operator!=(const ZipIterator& other) const
      {
         return !(*this == other);
      }

      reference operator*()
      {
         value = value_type(&*first_iter, &*second_iter);
         return value;
      }

      pointer operator->()
      {
         return &operator*();
      }
};

/**
 * Iterator range for 3-valued zip iterator.
 */
template <typename F, typename S, typename T=void>
class ZipIterRange
{
   public:
      typedef ZipIterator<typename F::iterator, typename S::iterator, typename T::iterator>
         value_type;

      /**
       * Construct from container types - initializes the range with ZipIterator(3)'s to begin() and
       * end() of the containers.
       */
      ZipIterRange(F& first, S& second, T& third)
         : it(first.begin(), second.begin(), third.begin() ),
           end(first.end(), second.end(), third.end() )
      { }

   private:
      value_type it;
      value_type end;

   public:
      /**
       * Convenience access method to the front of the range.
       */
      value_type& operator()()
      {
         return it;
      }

      /**
       * Check for empty range.
       */
      bool empty()
      {
         return it == end;
      }

      /**
       * Increment front of range.
       * Using this, a for-loop over the range can be implemented easily:
       * for (ZipIterRange<L1, L2, L3> range(l1, l2, l3); !range.empty(); ++range)
       *    doSomethingWithElement(*(range()->first), *(range()->second), *(range()->third) );
       */
      ZipIterRange& operator++()
      {
         ++it;
         return *this;
      }
};

/**
 * Iterator range for 2-valued zip iterator.
 */
template <typename F, typename S>
class ZipIterRange<F, S, void>
{
   public:
      typedef ZipIterator<typename F::iterator, typename S::iterator> value_type;

      /**
       * Construct from container types - initializes the range with ZipIter(2)'s to begin() and
       * end() of the containers.
       */
      ZipIterRange(F& first, S& second)
         : it(first.begin(), second.begin() ),
           end(first.end(), second.end() )
      { }

   private:
      value_type it;
      value_type end;

   public:
      /**
       * Convenience access method to the front of the range.
       */
      value_type& operator()()
      {
         return it;
      }

      /**
       * Check for empty range.
       */
      bool empty()
      {
         return it == end;
      }

      /**
       * Increment front of range.
       * Using this, a for-loop over the range can be implemented easily:
       * for (ZipIterRange<L1, L2, L3> range(l1, l2, l3); !range.empty(); ++range)
       *    doSomethingWithElement(*(range()->first), *(range()->second), *(range()->third));
       */
      ZipIterRange& operator++()
      {
         ++it;
         return *this;
      }
};

/**
 * Iterator range for 3-valued zip iterator consisting of const_iterators.
 */
template <typename F, typename S, typename T=void>
class ZipConstIterRange
{
   public:
      typedef ZipIterator<typename F::const_iterator, typename S::const_iterator,
              typename T::const_iterator>
         value_type;

      /**
       * Construct from container types - initializes the range with ZipIterator(3)'s to begin() and
       * end() of the containers.
       */
      ZipConstIterRange(const F& first, const S& second, const T& third)
         : it(first.begin(), second.begin(), third.begin() ),
           end(first.end(), second.end(), third.end() )
      { }

   private:
      value_type it;
      value_type end;

   public:
      /**
       * Convenience access method to the front of the range.
       */
      value_type& operator()()
      {
         return it;
      }

      /**
       * Check for empty range.
       */
      bool empty()
      {
         return it == end;
      }

      /**
       * Increment front of range.
       * Using this, a for-loop over the range can be implemented easily:
       * for (ZipIterRange<L1, L2, L3> range(l1, l2, l3); !range.empty(); ++range)
       *    doSomethingWithElement(*(range()->first), *(range()->second), *(range()->third) );
       */
      ZipConstIterRange& operator++()
      {
         ++it;
         return *this;
      }
};

/**
 * Iterator range for 2-valued zip iterator.
 */
template <typename F, typename S>
class ZipConstIterRange<F, S, void>
{
   public:
      typedef ZipIterator<typename F::const_iterator, typename S::const_iterator> value_type;

      /**
       * Construct from container types - initializes the range with ZipIter(2)'s to begin() and
       * end() of the containers.
       */
      ZipConstIterRange(const F& first, const S& second)
         : it(first.begin(), second.begin() ),
           end(first.end(), second.end() )
      { }

   private:
      value_type it;
      value_type end;

   public:
      /**
       * Convenience access method to the front of the range.
       */
      value_type& operator()()
      {
         return it;
      }

      /**
       * Check for empty range.
       */
      bool empty()
      {
         return it == end;
      }

      /**
       * Increment front of range.
       * Using this, a for-loop over the range can be implemented easily:
       * for (ZipIterRange<L1, L2, L3> range(l1, l2, l3); !range.empty(); ++range)
       *    doSomethingWithElement(*(range()->first), *(range()->second), *(range()->third));
       */
      ZipConstIterRange& operator++()
      {
         ++it;
         return *this;
      }
};


#endif /* ZIP_ITERATOR_H_ */
