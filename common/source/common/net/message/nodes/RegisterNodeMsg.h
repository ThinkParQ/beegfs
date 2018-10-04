#ifndef REGISTERNODEMSG_H_
#define REGISTERNODEMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/nodes/Node.h>

/**
 * Registers a new node with the management daemon.
 *
 * This also provides the mechanism to retrieve a numeric node ID from the management node.
 */
class RegisterNodeMsg : public NetMessageSerdes<RegisterNodeMsg>
{
   public:

      /**
       * @param nodeNumID set to 0 if no numeric ID was assigned yet and the numeric ID will be set
       * in the reponse
       * @param nicList just a reference, so do not free it as long as you use this object!
       * @param portUDP 0 means undefined
       * @param portTCP 0 means undefined
       */
      RegisterNodeMsg(const std::string& nodeID, NumNodeID nodeNumID, NodeType nodeType,
         NicAddressList* nicList,
         uint16_t portUDP, uint16_t portTCP) : BaseType(NETMSGTYPE_RegisterNode)
      {
         this->nodeID = nodeID;

         this->nodeNumID = nodeNumID;

         this->nodeType = nodeType;

         this->rootIsBuddyMirrored = false;

         this->instanceVersion = 0; // reserved for future use

         this->nicListVersion = 0; // reserved for future use
         this->nicList = nicList;

         this->portUDP = portUDP;
         this->portTCP = portTCP;
      }

      /**
       * For deserialization only
       */
      RegisterNodeMsg() : BaseType(NETMSGTYPE_RegisterNode)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->instanceVersion
            % obj->nicListVersion
            % obj->nodeID
            % serdes::backedPtr(obj->nicList, obj->parsed.nicList)
            % obj->nodeType
            % obj->nodeNumID
            % obj->rootNumID
            % obj->rootIsBuddyMirrored
            % obj->portUDP
            % obj->portTCP;
      }

   private:
      std::string nodeID;
      int32_t nodeType;
      NumNodeID nodeNumID; // 0 means "undefined"
      NumNodeID rootNumID; // 0 means "undefined"
      bool rootIsBuddyMirrored;
      uint64_t instanceVersion; // not used currently
      uint64_t nicListVersion; // not used currently
      uint16_t portUDP; // 0 means "undefined"
      uint16_t portTCP; // 0 means "undefined"

      // for serialization
      NicAddressList* nicList; // not owned by this object!

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

      void setRootNumID(NumNodeID rootNumID)
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

      uint16_t getPortUDP() const
      {
         return portUDP;
      }

      uint16_t getPortTCP() const
      {
         return portTCP;
      }

};


#endif /* REGISTERNODEMSG_H_ */
