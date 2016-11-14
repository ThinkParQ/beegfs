#ifndef NODE_H_
#define NODE_H_

#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>
#include <common/toolkit/Time.h>
#include <common/Common.h>
#include "NodeConnPool.h"

// forward declaration
class Node;


typedef std::list<Node*> NodeList;
typedef NodeList::iterator NodeListIter;


enum NodeType
   {NODETYPE_Invalid = 0, NODETYPE_Meta = 1, NODETYPE_Storage = 2, NODETYPE_Client = 3,
   NODETYPE_Mgmt = 4};


/**
 * Note: This class is thread-safe
 */
class Node
{
   public:
      Node(std::string nodeID, uint16_t nodeNumID, unsigned short portUDP, unsigned short portTCP,
         NicAddressList& nicList);
      virtual ~Node();

      void updateLastHeartbeatT();
      Time getLastHeartbeatT();
      bool waitForNewHeartbeatT(Time* oldT, int timeoutMS);
      void updateInterfaces(unsigned short portUDP, unsigned short portTCP,
         NicAddressList& nicList);

      std::string getTypedNodeID();
      std::string getNodeIDWithTypeStr();
      std::string getNodeTypeStr();
      
      // static
      static std::string getTypedNodeID(std::string nodeID, uint16_t nodeNumID, NodeType nodeType);
      static std::string getNodeIDWithTypeStr(std::string nodeID, uint16_t nodeNumID,
         NodeType nodeType);
      static std::string nodeTypeToStr(NodeType nodeType);


//      static std::string getNodeIDWithTypeStrUnreferenced(NodeStoreServers* nodeStore,
//         uint16_t nodeID);

      
      
   protected:
      Mutex mutex;
      Condition changeCond; // for last heartbeat time changes only 

      Node(std::string nodeID, uint16_t nodeNumID, unsigned short portUDP);
      
      void updateLastHeartbeatTUnlocked();
      Time getLastHeartbeatTUnlocked();
      void updateInterfacesUnlocked(unsigned short portUDP, unsigned short portTCP,
         NicAddressList& nicList);

      // getters & setters
      void setConnPool(NodeConnPool* connPool)
      {
         this->connPool = connPool;
      }
            
   
   private:
      std::string id; // string ID, generated locally on each node
      uint16_t numID; // numeric ID, assigned by mgmtd server store

      NodeType nodeType;

      unsigned fhgfsVersion; // fhgfs version code of this node

      NodeConnPool* connPool;
      unsigned short portUDP;

      Time lastHeartbeatT; // last heartbeat receive time
      
      
   public:
      // getters & setters
      std::string getID()
      {
         return id;
      }
      
      uint16_t getNumID()
      {
         return numID;
      }

      void setNumID(uint16_t numID)
      {
         this->numID = numID;
      }

      NicAddressList getNicList()
      {
         return connPool->getNicList();
      }
      
      NodeConnPool* getConnPool()
      {
         return connPool;
      }
      
      unsigned short getPortUDP()
      {
         unsigned short retVal;
         
         SafeMutexLock mutexLock(&mutex);
         
         retVal = this->portUDP;
         
         mutexLock.unlock();
         
         return retVal;
      }
      
      unsigned short getPortTCP()
      {
         return this->connPool->getStreamPort();
      }
      
      NodeType getNodeType()
      {
         return nodeType;
      }

      void setNodeType(NodeType nodeType)
      {
         this->nodeType = nodeType;
      }

      unsigned getFhgfsVersion()
      {
         return this->fhgfsVersion;
      }

      void setFhgfsVersion(unsigned fhgfsVersion)
      {
         this->fhgfsVersion = fhgfsVersion;
      }

};


#endif /*NODE_H_*/
