#ifndef POOLEDSOCKET_H_
#define POOLEDSOCKET_H_

#include <common/net/sock/Socket.h>
#include <common/toolkit/Time.h>


/**
 * This class provides special extensions for sockets in a NodeConnPool.
 */
class PooledSocket : public Socket
{
   public:
      virtual ~PooledSocket() {};


   protected:
      PooledSocket() : expireTimeStart(true)
      {
         this->available = false;
         this->closeOnRelease = false;
      }


   private:
      bool available; // == !acquired
      bool closeOnRelease; // if true, close this socket when it is released
      Time expireTimeStart; // 0 means "doesn't expire", otherwise time when conn was established


   public:
      // inliners

      /**
       * Tests whether this socket is set to expire and whether its expire time has been exceeded.
       *
       * @param expireSecs the time in seconds after which an expire-enabled socket expires.
       * @return true if this socket has expired.
       */
      bool getHasExpired(unsigned expireSecs)
      {
         if(likely(expireTimeStart.getIsZero() ) )
            return false;

         if(expireTimeStart.elapsedMS() > (expireSecs*1000) ) // "*1000" for milliseconds
            return true;

         return false;
      }

      // getters & setters
      bool isAvailable() const
      {
         return available;
      }

      void setAvailable(bool available)
      {
         this->available = available;
      }

      void setExpireTimeStart()
      {
         expireTimeStart.setToNow();
      }

      bool getHasExpirationTimer()
      {
         return !expireTimeStart.getIsZero();
      }

      bool isCloseOnRelease()
      {
         return closeOnRelease;
      }

      void setCloseOnRelease(bool v)
      {
         closeOnRelease = v;
      }
};


#endif /*POOLEDSOCKET_H_*/
