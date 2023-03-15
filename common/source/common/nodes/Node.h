#ifndef NODE_H_
#define NODE_H_

#include <common/Common.h>
#include <common/nodes/NumNodeID.h>
#include <common/threading/Condition.h>
#include <common/toolkit/BitStore.h>
#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/Time.h>

#include "NodeConnPool.h"
#include "NodeType.h"

#include <mutex>

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
      Node(NodeType nodeType, std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP,
         unsigned short portTCP, const NicAddressList& nicList);
      virtual ~Node();

      Node(const Node&) = delete;
      Node& operator=(const Node&) = delete;

      // disabled only because it is not needed at the moment.
      Node(Node&&) = delete;
      Node& operator=(Node&&) = delete;

      void updateLastHeartbeatT();
      Time getLastHeartbeatT();
      bool updateInterfaces(unsigned short portUDP, unsigned short portTCP,
         NicAddressList& nicList);

      std::string getTypedNodeID() const;
      std::string getNodeIDWithTypeStr() const;

      // static
      static std::string getTypedNodeID(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType);
      static std::string getNodeIDWithTypeStr(std::string nodeID, NumNodeID nodeNumID,
         NodeType nodeType);


   protected:
      Mutex mutex;

      NodeType nodeType;

      Node(NodeType nodeType, std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP);

      void updateLastHeartbeatTUnlocked();
      Time getLastHeartbeatTUnlocked();
      bool updateInterfacesUnlocked(unsigned short portUDP, unsigned short portTCP,
         NicAddressList& nicList);

      // getters & setters
      void setConnPool(NodeConnPool* connPool)
      {
         this->connPool = connPool;
      }


   private:
      std::string id; // string ID, generated locally on each node
      NumNodeID numID; // numeric ID, assigned by mgmtd server store

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
         const std::lock_guard<Mutex> lock(mutex);

         return this->portUDP;
      }

      virtual unsigned short getPortTCP()
      {
         return this->connPool->getStreamPort();
      }

      NodeType getNodeType() const
      {
         return nodeType;
      }


      struct VectorDes
      {
         bool v6;
         std::vector<NodeHandle>& nodes;

         Deserializer& runV6(Deserializer& des) const
         {
            uint32_t elemCount;
            uint32_t padding;

            des
               % elemCount
               % padding; // PADDING

            if(!des.good())
               return des;

            nodes.clear();

            while (nodes.size() != elemCount)
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
                  % fhgfsVersion
                  % nodeNumID
                  % portUDP
                  % portTCP
                  % nodeType;

               if(unlikely(!des.good()))
                  break;

               nodes.push_back(std::make_shared<Node>(NodeType(nodeType), nodeID, nodeNumID,
                     portUDP, portTCP, nicList));
            }

            return des;
         }

         Deserializer& run(Deserializer& des) const
         {
            uint32_t elemCount;

            des % elemCount;

            if(!des.good())
               return des;

            nodes.clear();

            while (nodes.size() != elemCount)
            {
               NicAddressList nicList;
               char nodeType = 0;
               uint16_t portTCP = 0;
               uint16_t portUDP = 0;
               NumNodeID nodeNumID;
               std::string nodeID;

               des
                  % nodeID
                  % nicList
                  % nodeNumID
                  % portUDP
                  % portTCP
                  % nodeType;

               if(unlikely(!des.good()))
                  break;

               nodes.push_back(std::make_shared<Node>(NodeType(nodeType), nodeID, nodeNumID,
                        portUDP, portTCP, nicList));
            }

            return des;
         }

         friend Deserializer& operator%(Deserializer& des, const VectorDes& value)
         {
            if (value.v6)
               return value.runV6(des);
            else
               return value.run(des);
         }
      };

      static VectorDes inV6Format(std::vector<NodeHandle>& nodes)
      {
         return {true, nodes};
      }
};

inline Serializer& operator%(Serializer& ser, const std::vector<NodeHandle>& nodes)
{
   ser % uint32_t(nodes.size());

   for (auto it = nodes.begin(), end = nodes.end(); it != end; ++it)
   {
      Node& node = **it;

      ser
         % node.getID()
         % node.getNicList()
         % node.getNumID()
         % node.getPortUDP()
         % node.getPortTCP()
         % char(node.getNodeType() );
   }

   return ser;
}

inline Deserializer& operator%(Deserializer& des, std::vector<NodeHandle>& nodes)
{
   Node::VectorDes mod = { false, nodes };
   return des % mod;
}

#endif /*NODE_H_*/
