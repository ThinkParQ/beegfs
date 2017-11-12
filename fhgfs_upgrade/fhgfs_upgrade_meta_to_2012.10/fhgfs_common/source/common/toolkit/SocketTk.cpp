#include "SocketTk.h"

bool SocketTk::getHostByName(const char* hostname, struct in_addr* outIPAddr)
{
   struct addrinfo hint;
   struct addrinfo* addressList;
   
   memset(&hint, 0, sizeof(struct addrinfo) );
   hint.ai_flags    = AI_CANONNAME;
   hint.ai_family   = PF_INET;
   hint.ai_socktype = SOCK_DGRAM;
   
   int getRes = getaddrinfo(hostname, NULL, &hint, &addressList);
   if(getRes)
      return false;
   
   struct sockaddr_in* inetAddr = (struct sockaddr_in*)(addressList->ai_addr);
   *outIPAddr = inetAddr->sin_addr;
   
   freeaddrinfo(addressList);
   
   return true;
}


