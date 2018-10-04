#ifndef NODECONNPOOL_H_
#define NODECONNPOOL_H_

#include <common/app/config/ICommonConfig.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/net/sock/PooledSocket.h>
#include <common/net/sock/StandardSocket.h>
#include <common/net/sock/RDMASocket.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/Common.h>

#include <mutex>

typedef std::list<PooledSocket*> ConnectionList;
typedef ConnectionList::iterator ConnListIter;

// forward declaration
class AbstractApp;
class Node;

/**
 * Holds statistics about currently established connections.
 */
struct NodeConnPoolStats
{
   unsigned numEstablishedStd;
   unsigned numEstablishedSDP;
   unsigned numEstablishedRDMA;
};

/**
 * Holds state of failed connects to avoid log spamming with conn errors.
 */
struct NodeConnPoolErrorState
{
   uint32_t lastSuccessPeerIP; // the last IP that we connected to successfully
   NicAddrType lastSuccessNicType; // the last nic type that we connected to successfully
   bool wasLastTimeCompleteFail; // true if last attempt failed on all routes

   bool getWasLastTimeCompleteFail()
   {
      return this->wasLastTimeCompleteFail;
   }

   void setCompleteFail()
   {
      this->lastSuccessPeerIP = 0;
      this->wasLastTimeCompleteFail = true;
   }

   void setConnSuccess(uint32_t lastSuccessPeerIP, NicAddrType lastSuccessNicType)
   {
      this->lastSuccessPeerIP = lastSuccessPeerIP;
      this->lastSuccessNicType = lastSuccessNicType;

      this->wasLastTimeCompleteFail = false;
   }

   bool equalsLastSuccess(uint32_t lastSuccessPeerIP, NicAddrType lastSuccessNicType)
   {
      return (this->lastSuccessPeerIP == lastSuccessPeerIP) &&
         (this->lastSuccessNicType == lastSuccessNicType);
   }

   bool isLastSuccessInitialized()
   {
      return (this->lastSuccessPeerIP != 0);
   }

   bool shouldPrintConnectedLogMsg(uint32_t currentPeerIP, NicAddrType currentNicType)
   {
      /* log only if we didn't succeed at all last time, or if we succeeded last time and it was
         with a different IP/NIC pair */

      return this->wasLastTimeCompleteFail ||
         !isLastSuccessInitialized() ||
         !equalsLastSuccess(currentPeerIP, currentNicType);
   }

   bool shouldPrintConnectFailedLogMsg(uint32_t currentPeerIP, NicAddrType currentNicType)
   {
      /* log only if this is the first connection attempt or if we succeeded last time this this
         IP/NIC pair */

      return (!this->wasLastTimeCompleteFail &&
         (!isLastSuccessInitialized() ||
            equalsLastSuccess(currentPeerIP, currentNicType) ) );
   }
};


/**
 * This class represents a pool of stream connections to a certain node.
 */
class NodeConnPool
{
   public:
      NodeConnPool(Node& parentNode, unsigned short streamPort, const NicAddressList& nicList);
      virtual ~NodeConnPool();

      NodeConnPool(const NodeConnPool&) = delete;
      NodeConnPool(NodeConnPool&&) = delete;
      NodeConnPool& operator=(const NodeConnPool&) = delete;
      NodeConnPool& operator=(NodeConnPool&&) = delete;

      Socket* acquireStreamSocket();
      virtual Socket* acquireStreamSocketEx(bool allowWaiting);
      virtual void releaseStreamSocket(Socket* sock);
      virtual void invalidateStreamSocket(Socket* sock);

      unsigned disconnectAndResetIdleStreams();

      void updateInterfaces(unsigned short streamPort, NicAddressList& nicList);

      bool getFirstPeerName(NicAddrType nicType, std::string* outPeerName, bool* outIsNonPrimary);


   private:
      NicAddressList nicList;
      ConnectionList connList;

      AbstractApp* app;
      Node& parentNode; // backlink to the node object to which this conn pool belongs
      unsigned short streamPort;
      NicListCapabilities localNicCaps;

      unsigned availableConns; // available established conns
      unsigned establishedConns; // not equal to connList.size!!
      unsigned maxConns;
      unsigned fallbackExpirationSecs; // expiration time for conns to fallback interfaces
      bool isChannelDirect;

      NodeConnPoolStats stats;
      NodeConnPoolErrorState errState;

      Mutex mutex;
      Condition changeCond;

      virtual void invalidateSpecificStreamSocket(Socket* sock);
      virtual unsigned invalidateAllAvailableStreams(bool idleStreamsOnly);
      void resetStreamsIdleFlag();
      void applySocketOptionsPreConnect(RDMASocket* sock);
      void applySocketOptionsPreConnect(StandardSocket* sock);
      void applySocketOptionsConnected(StandardSocket* sock);
      void authenticateChannel(Socket* sock);
      void makeChannelIndirect(Socket* sock);

      void statsAddNic(NicAddrType nicType);
      void statsRemoveNic(NicAddrType nicType);

   public:
      // getters & setters

      virtual NicAddressList getNicList()
      {
         const std::lock_guard<Mutex> lock(mutex);

         return nicList;
      }

      unsigned short getStreamPort()
      {
         const std::lock_guard<Mutex> lock(mutex);

         return streamPort;
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
         const std::lock_guard<Mutex> lock(mutex);

         this->maxConns = maxConns;
      }

      void getStats(NodeConnPoolStats* outStats)
      {
         const std::lock_guard<Mutex> lock(mutex);

         *outStats = this->stats;
      }

};

#endif /*NODECONNPOOL_H_*/
