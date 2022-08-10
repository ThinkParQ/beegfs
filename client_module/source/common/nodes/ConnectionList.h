#ifndef CONNECTIONLIST_H_
#define CONNECTIONLIST_H_

#include <common/Common.h>
#include <common/toolkit/list/PointerList.h>
#include <common/net/sock/PooledSocket.h>

struct ConnectionList;
typedef struct ConnectionList ConnectionList;


static inline void ConnectionList_init(ConnectionList* this, bool owner);
static inline void ConnectionList_uninit(ConnectionList* this);
static inline void ConnectionList_append(ConnectionList* this, PooledSocket* socket);
static inline void ConnectionList_prepend(ConnectionList* this, PooledSocket* socket);
static inline int ConnectionList_moveToHead(ConnectionList* this, PooledSocket* socket);
static inline int ConnectionList_moveToTail(ConnectionList* this, PooledSocket* socket);
static inline int ConnectionList_remove(ConnectionList* this, PooledSocket* socket);
static inline size_t ConnectionList_length(ConnectionList* this);


struct ConnectionList
{
   PointerList pointerList;
   bool owner;
};

/*
 * @param owner this list owns any PooledSockets added as list elements. The owner list
          maintains the socket's pool and poolElem members. A temporary list should set
          this parameter to false.
 */
void ConnectionList_init(ConnectionList* this, bool owner)
{
   PointerList_init( (PointerList*)this);
   this->owner = owner;
}

void ConnectionList_uninit(ConnectionList* this)
{
   PointerList_uninit( (PointerList*)this);
}

void ConnectionList_prepend(ConnectionList* this, PooledSocket* socket)
{
   PointerList_addHead( (PointerList*)this, socket);
   if (this->owner)
      PooledSocket_setPool(socket, this, PointerList_getHead( (PointerList*) this));
}

void ConnectionList_append(ConnectionList* this, PooledSocket* socket)
{
   PointerList_append( (PointerList*)this, socket);
   if (this->owner)
      PooledSocket_setPool(socket, this, PointerList_getTail( (PointerList*) this));
}

int ConnectionList_remove(ConnectionList* this, PooledSocket* socket)
{
   if (unlikely(PooledSocket_getPoolElem(socket) == NULL))
      return -EINVAL;
   PointerList_removeElem( (PointerList*)this, PooledSocket_getPoolElem(socket));
   if (this->owner)
      PooledSocket_setPool(socket, NULL, NULL);
   return 0;
}

int ConnectionList_moveToHead(ConnectionList* this, PooledSocket* socket)
{
   if (unlikely(PooledSocket_getPoolElem(socket) == NULL))
      return -EINVAL;
   PointerList_moveToHead( (PointerList*) this, PooledSocket_getPoolElem(socket));
   return 0;
}

int ConnectionList_moveToTail(ConnectionList* this, PooledSocket* socket)
{
   if (unlikely(PooledSocket_getPoolElem(socket) == NULL))
      return -EINVAL;
   PointerList_moveToTail( (PointerList*) this, PooledSocket_getPoolElem(socket));
   return 0;
}

static inline size_t ConnectionList_length(ConnectionList* this)
{
   return PointerList_length( (PointerList*)this);
}


#endif /*CONNECTIONLIST_H_*/
