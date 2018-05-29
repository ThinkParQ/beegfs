#include <common/toolkit/SocketTk.h>
#include <common/net/sock/Socket.h>
#include <common/threading/Thread.h>
#include <linux/in.h>

void _Socket_init(Socket* this)
{
   memset(this, 0, sizeof(*this) );

   this->sockType = NICADDRTYPE_STANDARD;
}

void _Socket_uninit(Socket* this)
{
   SAFE_KFREE(this->peername);
}


bool Socket_bind(Socket* this, unsigned short port)
{
   struct in_addr ipAddr = { INADDR_ANY };

   return this->ops->bindToAddr(this, &ipAddr, port);
}

bool Socket_bindToAddr(Socket* this, struct in_addr* ipAddr, unsigned short port)
{
   return this->ops->bindToAddr(this, ipAddr, port);
}
