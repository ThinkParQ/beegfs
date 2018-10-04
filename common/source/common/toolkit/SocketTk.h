#ifndef SOCKETTK_H_
#define SOCKETTK_H_

#include <common/Common.h>
#include <nu/error_or.hpp>
#include <system_error>

const std::error_category& gai_category();

inline std::error_code make_gai_error_code(int condition, int err)
{
   if (condition == EAI_SYSTEM)
      return std::error_code(err, std::system_category());
   else
      return std::error_code(condition, gai_category());
}

class SocketTk
{
   private:
      SocketTk() {}

   public:
      static nu::error_or<in_addr> getHostByName(const std::string& hostname);
      static std::string getHostnameFromIP(struct in_addr* ipAddr, bool nameRequired,
         bool fullyQualified);
};

#endif /*SOCKETTK_H_*/
