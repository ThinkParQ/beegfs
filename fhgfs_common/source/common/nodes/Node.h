#ifndef NODE_H_
#define NODE_H_

#include <common/Common.h>
#include <common/nodes/NodeFeatureFlags.h>
#include <common/nodes/NumNodeID.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>
#include <common/toolkit/BitStore.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/Time.h>

#include "NodeConnPool.h"
#include "NodeType.h"

// forward declaration
class Node;

typedef std::shared_ptr<Node> NodeHandle;

typedef std::list<Node*> NodeList;
typedef NodeList::iterator NodeListIter;


/**
 * This class represents a metadata, storage, client, etc node (aka service). It contains things
 * like ID and feature flags of a node and also provides the connection pool to communicate with
 * that node.
 */
class Node
{
   public:
      Node(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP, unsigned short portTCP,
         const NicAddressList& nicList);
      virtual ~Node();

      Node(const Node&) = delete;
      Node& operator=(const Node&) = delete;

      // disabled only because it is not needed at the moment.
      Node(Node&&) = delete;
      Node& operator=(Node&&) = delete;

      void updateLastHeartbeatT();
      Time getLastHeartbeatT();
      bool waitForNewHeartbeatT(Time* oldT, int timeoutMS);
      void updateInterfaces(unsigned short portUDP, unsigned short portTCP,
         NicAddressList& nicList);

      std::string getTypedNodeID() const;
      std::string getNodeIDWithTypeStr() const;
      std::string getNodeTypeStr() const;

      void setFeatureFlags(const BitStore* featureFlags);

      // static
      static std::string getTypedNodeID(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType);
      static std::string getNodeIDWithTypeStr(std::string nodeID, NumNodeID nodeNumID,
         NodeType nodeType);
      static std::string nodeTypeToStr(NodeType nodeType);


   protected:
      Mutex mutex;
      Condition changeCond; // for last heartbeat time changes only

      NodeType nodeType;

      Node(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP);

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
      NumNodeID numID; // numeric ID, assigned by mgmtd server store

      unsigned fhgfsVersion; // fhgfs version code of this node
      BitStore nodeFeatureFlags; /* supported features of this node (access not protected by mutex,
                                    so be careful with updates) */

      NodeConnPool* connPool;
      unsigned short portUDP;

      Time lastHeartbeatT; // last heartbeat receive time


   public:
      // getters & setters

      const std::string& getID()
      {
         return id;
      }

      NumNodeID getNumID() const
      {
         return numID;
      }

      void setNumID(NumNodeID numID)
      {
         this->numID = numID;
      }

      NicAddressList getNicList()
      {
         return connPool->getNicList();
      }

      NodeConnPool* getConnPool() const
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

      virtual unsigned short getPortTCP()
      {
         return this->connPool->getStreamPort();
      }

      NodeType getNodeType() const
      {
         return nodeType;
      }

      void setNodeType(NodeType nodeType)
      {
         this->nodeType = nodeType;
      }

      unsigned getFhgfsVersion() const
      {
         return this->fhgfsVersion;
      }

      void setFhgfsVersion(unsigned fhgfsVersion)
      {
         this->fhgfsVersion = fhgfsVersion;
      }

      /**
       * Check if this node supports a certain feature.
       */
      bool hasFeature(unsigned featureBitIndex)
      {
         SafeMutexLock lock(&mutex);

         bool result = nodeFeatureFlags.getBitNonAtomic(featureBitIndex);

         lock.unlock();

         return result;
      }

      /**
       * Add a feature flag to this node.
       */
      void addFeature(unsigned featureBitIndex)
      {
         SafeMutexLock lock(&mutex);

         nodeFeatureFlags.setBit(featureBitIndex, true);

         lock.unlock();
      }

      /**
       * note: returns a reference to internal flags, so this is only valid while you hold a
       * reference to this node.
       */
      const BitStore* getNodeFeatures()
      {
         return &nodeFeatureFlags;
      }



      struct VectorDes
      {
         bool longNodeIDs;
         std::vector<NodeHandle>& nodes;

         friend Deserializer& operator%(Deserializer& des, const VectorDes& value)
         {
            uint32_t elemCount;
            uint32_t padding;

            des
               % elemCount
               % padding; // PADDING

            if(!des.good())
               return des;

            value.nodes.clear();

            while (value.nodes.size() != elemCount)
            {
               // PADDING
               PadFieldTo<Deserializer> pad(des, 8);

               NicAddressList nicList;
               char nodeType = 0;
               unsigned fhgfsVersion = 0;
               BitStore nodeFeatureFlags;
               uint16_t portTCP = 0;
               uint16_t portUDP = 0;
               NumNodeID nodeNumID;
               std::string nodeID;

               des
                  % nodeFeatureFlags
                  % serdes::stringAlign4(nodeID)
                  % nicList
                  % fhgfsVersion;

               if (value.longNodeIDs)
               {
                  des % nodeNumID;
               }
               else
               {
                  uint16_t id;
                  des % id;
                  nodeNumID = NumNodeID(id);
               }

               des
                  % portUDP
                  % portTCP
                  % nodeType;

               if(unlikely(!des.good()))
                  break;

               value.nodes.push_back(std::make_shared<Node>(nodeID, nodeNumID, portUDP, portTCP,
                     nicList));
               value.nodes.back()->setNodeType(NodeType(nodeType) );
               value.nodes.back()->setFhgfsVersion(fhgfsVersion);
               value.nodes.back()->setFeatureFlags(&nodeFeatureFlags);
            }

            return des;
         }
      };

      static VectorDes vectorWithShortIDs(std::vector<NodeHandle>& nodes)
      {
         VectorDes result = { false, nodes };
         return result;
      }
};

inline Serializer& operator%(Serializer& ser, const std::vector<NodeHandle>& nodes)
{
   ser
      % uint32_t(nodes.size() )
      % uint32_t(0); // PADDING

   for (auto it = nodes.begin(), end = nodes.end(); it != end; ++it)
   {
      // PADDING
      PadFieldTo<Serializer> pad(ser, 8);

      Node& node = **it;

      ser
         % *node.getNodeFeatures()
         % serdes::stringAlign4(node.getID() )
         % node.getNicList()
         % node.getFhgfsVersion()
         % node.getNumID()
         % node.getPortUDP()
         % node.getPortTCP()
         % char(node.getNodeType() );
   }

   return ser;
}

inline Deserializer& operator%(Deserializer& des, std::vector<NodeHandle>& nodes)
{
   Node::VectorDes mod = { true, nodes };
   return des % mod;
}

#endif /*NODE_H_*/
