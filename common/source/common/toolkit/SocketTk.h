#ifndef SOCKETTK_H_
#define SOCKETTK_H_

#include <common/Common.h>


class SocketTk
{
   private:
      SocketTk() {}

   public:
      static bool getHostByName(const char* hostname, struct in_addr* outIPAddr);
      static std::string getHostnameFromIP(struct in_addr* ipAddr, bool nameRequired,
         bool fullyQualified);
};

#endif /*SOCKETTK_H_*/
