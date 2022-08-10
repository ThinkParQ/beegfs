#ifndef POOLEDSOCKET_H_
#define POOLEDSOCKET_H_

#include <common/net/sock/Socket.h>
#include <common/toolkit/Time.h>


struct PooledSocket;
typedef struct PooledSocket PooledSocket;
struct ConnectionList;
typedef struct ConnectionList ConnectionList;


static inline void _PooledSocket_init(PooledSocket* this, NicAddrType_t nicType);
static inline void _PooledSocket_uninit(Socket* this);

// inliners
static inline bool PooledSocket_getHasExpired(PooledSocket* this, unsigned expireSecs);

// getters & setters
static inline bool PooledSocket_isAvailable(PooledSocket* this);
static inline void PooledSocket_setAvailable(PooledSocket* this, bool available);
static inline bool PooledSocket_getHasActivity(PooledSocket* this);
static inline void PooledSocket_setHasActivity(PooledSocket* this);
static inline void PooledSocket_resetHasActivity(PooledSocket* this);
static inline bool PooledSocket_getHasExpirationTimer(PooledSocket* this);
static inline void PooledSocket_setExpireTimeStart(PooledSocket* this);
static inline NicAddrType_t PooledSocket_getNicType(PooledSocket* this);
static inline ConnectionList* PooledSocket_getPool(PooledSocket* this);
static inline PointerListElem* PooledSocket_getPoolElem(PooledSocket* this);
static inline void PooledSocket_setPool(PooledSocket* this, ConnectionList* pool,
   PointerListElem* poolElem);


/**
 * This class provides special extensions for sockets in a NodeConnPool.
 */
struct PooledSocket
{
   Socket socket;
   ConnectionList* pool;
   PointerListElem* poolElem;
   bool available; // == !acquired
   bool hasActivity; // true if channel was not idle (part of channel class in fhgfs_common)
   bool closeOnRelease; /* release must close socket. used for signal handling */
   Time expireTimeStart; // 0 means "doesn't expire", otherwise time when conn was established
   NicAddrType_t nicType; // same as the interface for which this conn was established
};


void _PooledSocket_init(PooledSocket* this, NicAddrType_t nicType)
{
   _Socket_init( (Socket*)this);

   this->available = false;
   this->hasActivity = true; // initially active to avoid immediate disconnection
   this->closeOnRelease = false;
   Time_initZero(&this->expireTimeStart);
   this->nicType = nicType;
   this->pool = NULL;
   this->poolElem = NULL;
}

void _PooledSocket_uninit(Socket* this)
{
   _Socket_uninit(this);
}

/**
 * Tests whether this socket is set to expire and whether its expire time has been exceeded.
 *
 * @param expireSecs the time in seconds after which an expire-enabled socket expires.
 * @return true if this socket has expired.
 */
bool PooledSocket_getHasExpired(PooledSocket* this, unsigned expireSecs)
{
   if(likely(Time_getIsZero(&this->expireTimeStart) ) )
      return false;

   if(Time_elapsedMS(&this->expireTimeStart) > (expireSecs*1000) ) // "*1000" for milliseconds
      return true;

   return false;
}

bool PooledSocket_isAvailable(PooledSocket* this)
{
   return this->available;
}

void PooledSocket_setAvailable(PooledSocket* this, bool available)
{
   this->available = available;
}

bool PooledSocket_getHasActivity(PooledSocket* this)
{
   return this->hasActivity;
}

void PooledSocket_setHasActivity(PooledSocket* this)
{
   this->hasActivity = true;
}

void PooledSocket_resetHasActivity(PooledSocket* this)
{
   this->hasActivity = false;
}

bool PooledSocket_getHasExpirationTimer(PooledSocket* this)
{
   return !Time_getIsZero(&this->expireTimeStart);
}

void PooledSocket_setExpireTimeStart(PooledSocket* this)
{
   Time_setToNow(&this->expireTimeStart);
}

NicAddrType_t PooledSocket_getNicType(PooledSocket* this)
{
   return this->nicType;
}

void PooledSocket_setPool(PooledSocket* this, ConnectionList* pool,
   PointerListElem* poolElem)
{
   this->pool = pool;
   this->poolElem = poolElem;
}

ConnectionList* PooledSocket_getPool(PooledSocket* this)
{
   return this->pool;
}

PointerListElem* PooledSocket_getPoolElem(PooledSocket* this)
{
   return this->poolElem;
}

#endif /*POOLEDSOCKET_H_*/
