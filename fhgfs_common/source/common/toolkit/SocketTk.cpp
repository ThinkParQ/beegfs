#include "SocketTk.h"

#include <sys/socket.h>
#include <netdb.h>


#define SOCKETTK_MAX_HOSTNAME_RESOLVE_LEN 128

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

/**
 * Resolve hostname for a given IP address.
 *
 * @param nameRequired true if hostname is required an no numeric representation is allowed to be
 * returned.
 * @param fullyQualified true if the fully qualified domain name should be returned.
 * @return resolved hostname for given IP; empty string on error, IP address as string if hostname
 * couldn't be resolved.
 */
std::string SocketTk::getHostnameFromIP(struct in_addr* ipAddr, bool nameRequired,
   bool fullyQualified)
{
   char hostname[SOCKETTK_MAX_HOSTNAME_RESOLVE_LEN];

   struct sockaddr_in serv_addr;

   memset(&serv_addr, 0, sizeof(serv_addr) );

   serv_addr.sin_family        = AF_INET;
   serv_addr.sin_addr.s_addr   = ipAddr->s_addr;

   int getInfoFlags = 0;

   if(nameRequired)
      getInfoFlags |= NI_NAMEREQD;

   if(!fullyQualified)
      getInfoFlags |= NI_NOFQDN;

   int getRes = getnameinfo( (struct sockaddr*)&serv_addr, sizeof(serv_addr), hostname,
      sizeof(hostname), NULL, 0, getInfoFlags);

   if(!getRes)
      return hostname;

   return "";
}
