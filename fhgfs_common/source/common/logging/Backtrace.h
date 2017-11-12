#ifndef COMMON_BACKTRACE_H
#define COMMON_BACKTRACE_H

/**
 * Helper class to easily log a fragment of the current backtrace inline.
 */

#include <execinfo.h>
#include <memory>
#include <sstream>

template<unsigned LEN>
class Backtrace
{
   public:
      Backtrace() __attribute__((noinline))
      {
         void* btbuf[LEN + 2];
         auto btlen = backtrace(&btbuf[0], LEN + 2);
         std::unique_ptr<char*, free_delete<char*>> symbols(backtrace_symbols(&btbuf[0], btlen));

         if (!symbols || btlen < 2)
         {
            bt = "(error)";
         }
         else
         {
            std::ostringstream oss;
            for (int i = 2; i < btlen - 1; i++)
               oss << symbols.get()[i] << "; ";
            oss << symbols.get()[btlen - 1] << ".";

            bt = oss.str();
         }
      }

      const std::string& str() const { return bt; }

   private:
      std::string bt;

      template<class T> struct free_delete { void operator()(T* p) { free(p); } };
};

template<>
class Backtrace<0> { public: Backtrace() = delete; };

template<unsigned LEN>
std::ostream& operator<<(std::ostream& os, const Backtrace<LEN>& bt)
{
   os << bt.str();
   return os;
}

#endif /* COMMON_BACKTRACE_H */
