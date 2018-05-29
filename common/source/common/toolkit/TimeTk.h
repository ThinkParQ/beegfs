#ifndef TIMETK_H
#define TIMETK_H 

#include <chrono>
#include <string>

namespace TimeTk {

using highest_resolution_steady_clock =
   std::conditional<std::chrono::high_resolution_clock::is_steady,
         std::chrono::high_resolution_clock,
         std::chrono::steady_clock>::type;

//static_assert (highest_resolution_steady_clock::is_steady, "steady clock should be steady");

namespace detail {
   template<typename Duration>
   struct as_double_duration;

   template<typename Rep, typename Period>
   struct as_double_duration<std::chrono::duration<Rep, Period>>
   {
      using type = std::chrono::duration<double, Period>;
   };
}

template<typename Duration>
   using as_double_duration = typename detail::as_double_duration<Duration>::type;

namespace detail {

   template<typename Period>
   inline std::string chrono_unit_for_period ()
   {
      if (Period::den == 1)
      {
         return "[" + std::to_string (Period::num) + "]s";
      }
      else
      {
         return "[" + std::to_string (Period::num) + "/"
            + std::to_string (Period::den) + "]s";
      }
   }

#define CHRONO_UNIT_FOR_SI_PERIOD(symbol_, ratio_)      \
   template<>                                           \
   inline std::string chrono_unit_for_period<ratio_> () \
   {                                                    \
      return #symbol_;                                  \
   }

   CHRONO_UNIT_FOR_SI_PERIOD (ns, std::chrono::nanoseconds::period);
   CHRONO_UNIT_FOR_SI_PERIOD (µs, std::chrono::microseconds::period);
   CHRONO_UNIT_FOR_SI_PERIOD (ms, std::chrono::milliseconds::period);
   CHRONO_UNIT_FOR_SI_PERIOD (s, std::chrono::seconds::period);
   CHRONO_UNIT_FOR_SI_PERIOD (min, std::chrono::minutes::period);
   CHRONO_UNIT_FOR_SI_PERIOD (hr, std::chrono::hours::period);

#undef CHRONO_UNIT_FOR_SI_PERIOD
} // namespace detail

template<typename Rep, typename Period>
std::ostream& operator<< (std::ostream& os, std::chrono::duration<Rep, Period> duration)
{
   return os << duration.count() << " " << detail::chrono_unit_for_period<Period>();
}


template<typename Duration>
struct print_nice_time_t {
   Duration const& _duration;
   print_nice_time_t (Duration const& duration) : _duration (duration) {}
};


template<typename Duration>
std::ostream& operator<< (std::ostream& os, print_nice_time_t<Duration> const& x) {
#define NICE_CASE(unit_, count_)                                            \
   if (x._duration > std::chrono::unit_ (count_))                           \
      return os << std::chrono::duration_cast<std::chrono::duration<double, \
            std::chrono::unit_::period>> (x._duration)

   NICE_CASE(hours, 3);
   NICE_CASE(minutes, 3);
   NICE_CASE(seconds, 3);
   NICE_CASE(milliseconds, 3);
   NICE_CASE(microseconds, 3);

#undef NICE_CASE

   return os << x._duration;
}

template<typename Duration>
print_nice_time_t<Duration> print_nice_time (Duration const& duration) {
   return {duration};
}

} // namespace TimeTk
#endif /* TIMETK_H */
