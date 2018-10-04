#ifndef HEARTBEATMSG_H_
#define HEARTBEATMSG_H_

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/nodes/Node.h>
#include <common/Common.h>

class HeartbeatMsg : public AcknowledgeableMsgSerdes<HeartbeatMsg>
{
   public:

      /**
       * @param nicList just a reference, so do not free it as long as you use this object
       */
      HeartbeatMsg(const std::string& nodeID, NumNodeID nodeNumID, NodeType nodeType,
         NicAddressList* nicList)
         : BaseType(NETMSGTYPE_Heartbeat)
      {
         this->nodeID = nodeID;
         this->nodeNumID = nodeNumID;

         this->nodeType = nodeType;

         this->rootIsBuddyMirrored = false;

         this->instanceVersion = 0; // reserved for future use

         this->nicListVersion = 0; // reserved for future use
         this->nicList = nicList;

         this->portUDP = 0; // 0 means "undefined"
         this->portTCP = 0; // 0 means "undefined"
      }

      /**
       * For deserialization only
       */
      HeartbeatMsg() : BaseType(NETMSGTYPE_Heartbeat)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->instanceVersion
            % obj->nicListVersion
            % obj->nodeType
            % obj->nodeID;

         obj->serializeAckID(ctx, 4);

         ctx
            % obj->nodeNumID
            % obj->rootNumID
            % obj->rootIsBuddyMirrored
            % obj->portUDP
            % obj->portTCP
            % serdes::backedPtr(obj->nicList, obj->parsed.nicList);
      }

   private:
      std::string nodeID;
      int32_t nodeType;
      NumNodeID nodeNumID;
      NumNodeID rootNumID; // 0 means unknown/undefined
      bool rootIsBuddyMirrored;
      uint64_t instanceVersion; // not used currently
      uint64_t nicListVersion; // not used currently
      uint16_t portUDP; // 0 means "undefined"
      uint16_t portTCP; // 0 means "undefined"

      // for serialization
      NicAddressList* nicList; // not owned by this object

      // for deserialization
      struct {
         NicAddressList nicList;
      } parsed;


   public:
      NicAddressList& getNicList()
      {
         return *nicList;
      }

      const std::string& getNodeID() const
      {
         return nodeID;
      }

      NumNodeID getNodeNumID() const
      {
         return nodeNumID;
      }

      NodeType getNodeType() const
      {
         return (NodeType)nodeType;
      }

      NumNodeID getRootNumID() const
      {
         return rootNumID;
      }

      void setRootNumID(const NumNodeID rootNumID)
      {
         this->rootNumID = rootNumID;
      }

      bool getRootIsBuddyMirrored()
      {
         return rootIsBuddyMirrored;
      }

      void setRootIsBuddyMirrored(bool rootIsBuddyMirrored)
      {
         this->rootIsBuddyMirrored = rootIsBuddyMirrored;
      }

      void setPorts(const uint16_t portUDP, const uint16_t portTCP)
      {
         this->portUDP = portUDP;
         this->portTCP = portTCP;
      }

      uint16_t getPortUDP() const
      {
         return portUDP;
      }

      uint16_t getPortTCP() const
      {
         return portTCP;
      }

};

#endif /*HEARTBEATMSG_H_*/
