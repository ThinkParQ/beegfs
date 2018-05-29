#include "NamedSocket.h"


#define NAMEDSOCKET_CONNECT_TIMEOUT_MS    5000


/**
 * @throw SocketException
 */
void NamedSocket::bindToPath()
{
   struct sockaddr_un localaddr_un;
   memset(&localaddr_un, 0, sizeof(localaddr_un) );

   localaddr_un.sun_family = this->getSockDomain();
   strcpy(localaddr_un.sun_path, this->path.c_str() );

   int bindRes = ::bind(sock, (struct sockaddr *)&localaddr_un, sizeof(localaddr_un) );
   if (bindRes == -1)
      throw SocketException("Unable to bind to named socket: " + this->path +
         ". SysErr: " + System::getErrString() );

   peername = std::string("Listen(named socket: ") + this->path + std::string(")");
}

/**
 * Overrides bindToAddr() function of a socket and ignores the input values, because this values are
 * not required for a NameSocket.
 *
 * @param ipAddr The IP address of the destination node (is ignored).
 * @param port The network port of the destination node (is ignored).
 * @throw SocketException
 */
void NamedSocket::bindToAddr(in_addr_t ipAddr, unsigned short port)
{
   bindToPath();
}

/**
 * @throw SocketException
 */
Socket* NamedSocket::accept(struct sockaddr *addr, socklen_t *addrlen)
{
   int acceptRes = ::accept(sock, addr, addrlen);
   if(acceptRes == -1)
   {
      throw SocketException(std::string("Error during socket accept(): ") +
         System::getErrString() );
   }

   try
   {
      Socket* acceptedSock = new NamedSocket(acceptRes, sockDomain, path);
      return acceptedSock;
   }
   catch(SocketException& e)
   {
      close(acceptRes);
      throw;
   }
}

void NamedSocket::connectToPath()
{
   const int timeoutMS = NAMEDSOCKET_CONNECT_TIMEOUT_MS;

   struct sockaddr_un localaddr_un;
   memset(&localaddr_un, 0, sizeof(localaddr_un) );

   localaddr_un.sun_family = this->getSockDomain();
   strcpy(localaddr_un.sun_path, this->path.c_str() );

   int flagsOrig = fcntl(sock, F_GETFL, 0);
   fcntl(sock, F_SETFL, flagsOrig | O_NONBLOCK); // make the socket nonblocking

   int connRes = ::connect(sock, (struct sockaddr*)&localaddr_un, sizeof(localaddr_un));
   if(connRes)
   {
      if(errno == EINPROGRESS)
      { // wait for "ready to send data"
         struct pollfd pollStruct = {sock, POLLOUT, 0};
         int pollRes = poll(&pollStruct, 1, timeoutMS);

         if(pollRes > 0)
         { // we got something (could also be an error)

            /* note: it's important to test ERR/HUP/NVAL here instead of POLLOUT only, because
               POLLOUT and POLLERR can be returned together. */

            if(pollStruct.revents & (POLLERR | POLLHUP | POLLNVAL) )
               throw SocketConnectException(std::string("Unable to establish connection: ") + path);

            // connection successfully established

            fcntl(sock, F_SETFL, flagsOrig);  // set socket back to original mode
            return;
         }
         else
         if(!pollRes)
            throw SocketConnectException(std::string("Timeout connecting to: ") + path);
         else
            throw SocketConnectException(std::string("Error connecting to: ") + path + ". "
               "SysErr: " + System::getErrString() );
      }
   }
   else
   { // immediate connect => strange, but okay...
      fcntl(sock, F_SETFL, flagsOrig);  // set socket back to original mode
      return;
   }

   throw SocketConnectException(std::string("Unable to connect to: ") + path +
      std::string(". SysErr: ") + System::getErrString() );
}

/**
 * Overrides connect() function of a socket and ignores the input values, because this values are
 * not required for a NameSocket.
 *
 * @param hostname The hostname of the destination node (is ignored).
 * @param port The network port of the destination node (is ignored).
 * @throw SocketException
 */
void NamedSocket::connect(const char* hostname, unsigned short port)
{
   connectToPath();
}

/**
 * Overrides connect() function of a socket and ignores the input values, because this values are
 * not required for a NameSocket.
 *
 * @param serv_addr The address of the destination node (is ignored).
 * @param addrlen The length of the server address.
 * @throw SocketException
 */
void NamedSocket::connect(const struct sockaddr* serv_addr, socklen_t addrlen)
{
   connectToPath();
}
