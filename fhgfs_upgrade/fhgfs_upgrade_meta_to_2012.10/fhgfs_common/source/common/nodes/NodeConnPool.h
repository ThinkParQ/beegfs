#ifndef NODECONNPOOL_H_
#define NODECONNPOOL_H_

#include <common/app/config/ICommonConfig.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/PooledSocket.h>
#include <common/net/sock/StandardSocket.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/Common.h>


typedef std::list<PooledSocket*> ConnectionList;
typedef ConnectionList::iterator ConnListIter;

// forward declaration
class AbstractApp;


/**
 * Note: This class is thread-safe
 */
class NodeConnPool
{
   public:
      NodeConnPool(unsigned short streamPort, NicAddressList& nicList);
      virtual ~NodeConnPool();
      
      Socket* acquireStreamSocket();
      virtual Socket* acquireStreamSocketEx(bool allowWaiting);
      virtual void releaseStreamSocket(Socket* sock);
      virtual void invalidateStreamSocket(Socket* sock);
      
      void updateInterfaces(unsigned short streamPort, NicAddressList& nicList);
      
      
   private:
      NicAddressList nicList; // must not be changed (rwlock would be an option otherwise)
      ConnectionList connList;

      AbstractApp* app;
      unsigned short streamPort;
      NicListCapabilities localNicCaps;
      
      unsigned availableConns; // available established conns
      unsigned establishedConns; // not equal to connList.size!!
      unsigned maxConns;
      int nonPrimaryExpirationCount; // expiration counter for conns over non-primary interfaces
      bool isChannelDirect;
      
      Mutex mutex;
      Condition changeCond;
      
      virtual void invalidateSpecificStreamSocket(Socket* sock);
      virtual void invalidateAllAvailableStreams();
      void applySocketOptionsPreConnect(StandardSocket* sock);
      void applySocketOptionsConnected(StandardSocket* sock);
      void makeChannelIndirect(Socket* sock);

      
   public:
      // getters & setters
      
      virtual NicAddressList getNicList()
      {
         SafeMutexLock mutexLock(&mutex); // L O C K
         
         NicAddressList nicListCopy(nicList);
         
         mutexLock.unlock(); // U N L O C K
         
         return nicListCopy;
      }
      
      unsigned short getStreamPort()
      {
         SafeMutexLock mutexLock(&mutex); // L O C K
         
         unsigned short retVal = streamPort;
         
         mutexLock.unlock(); // U N L O C K
         
         return retVal;
      }
      
      /**
       * @param localNicCaps will be copied
       */
      void setLocalNicCaps(NicListCapabilities* localNicCaps)
      {
         this->localNicCaps = *localNicCaps;
      }
      
      void setChannelDirect(bool isChannelDirect)
      {
         this->isChannelDirect = isChannelDirect;
      }
      
      /**
       * Note: This is not intended to be called under normal circumstances (it exists only for
       * special tests e.g. in fhgfs_online_cfg)
       */
      void setMaxConns(unsigned maxConns)
      {
         SafeMutexLock mutexLock(&mutex); // L O C K

         this->maxConns = maxConns;

         mutexLock.unlock(); // U N L O C K
      }
      
};

#endif /*NODECONNPOOL_H_*/
