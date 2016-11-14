#ifndef POOLEDSOCKET_H_
#define POOLEDSOCKET_H_

#include <common/net/sock/Socket.h>

class PooledSocket : public Socket
{
   public:
      virtual ~PooledSocket() {};
      
   protected:
      PooledSocket()
      {
         this->available = false;
         this->expirationCounter = 0;
      }
      
      
   private:
      bool available; // == !acquired
      int expirationCounter; // 0 means "doesn't expire", >0 means "numRequests until expiration"
      
      
   public:
      // inliners
      
      /**
       * Tests the expiration counter and decreases its value if necessary. 
       * 
       * @return true if this socket just expired
       */
      bool testAndDecExpirationCounter()
      {
         if(likely(!expirationCounter) )
            return false;
         
         if(expirationCounter == 1)
            return true; // we don't decrease the value here to keep the expiration stable
         
         expirationCounter--;
         
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
      
      void setExpirationCounter(int expirationCounter)
      {
         this->expirationCounter = expirationCounter;
      }
};


#endif /*POOLEDSOCKET_H_*/
