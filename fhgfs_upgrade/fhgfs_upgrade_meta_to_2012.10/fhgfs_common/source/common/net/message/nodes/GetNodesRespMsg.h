#ifndef GETNODESRESPMSG_H_
#define GETNODESRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/Common.h>


class GetNodesRespMsg : public NetMessage
{
   public:
      
      /**
       * @param rootID just a reference, so do not free it as long as you use this object!
       * @param nodeList just a reference, so do not free it as long as you use this object!
       */
      GetNodesRespMsg(uint16_t rootNumID, NodeList* nodeList) :
         NetMessage(NETMSGTYPE_GetNodesResp)
      {
         this->rootNumID = rootNumID;
         
         this->nodeList = nodeList;
      }

      /**
       * For deserialization only
       */
      GetNodesRespMsg() : NetMessage(NETMSGTYPE_GetNodesResp)
      {
      }

      
   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUShort() + // rootNumID
            Serialization::serialLenNodeList(nodeList);
      }


   private:
      uint16_t rootNumID;
      
      // for serialization
      NodeList* nodeList; // not owned by this object!

      // for deserialization
      unsigned nodeListElemNum;
      const char* nodeListStart;
      

   public:
   
      // inliners   

      /**
       * @return outNodesList each node in the list will be constructed and has to be deleted later
       */ 
      void parseNodeList(NodeList* outNodeList)
      {
         Serialization::deserializeNodeList(nodeListElemNum, nodeListStart, outNodeList);
      }
      
      // getters & setters
      uint16_t getRootNumID()
      {
         return rootNumID;
      }

};

#endif /*GETNODESRESPMSG_H_*/
