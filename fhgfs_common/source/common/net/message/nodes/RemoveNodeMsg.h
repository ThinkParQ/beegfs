#ifndef REMOVENODEMSG_H_
#define REMOVENODEMSG_H_

#include <common/Common.h>
#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/Node.h>

class RemoveNodeMsg : public AcknowledgeableMsgSerdes<RemoveNodeMsg>
{
   public:
      /**
       * @param nodeType NODETYPE_...
       */
      RemoveNodeMsg(NumNodeID nodeNumID, NodeType nodeType) :
         BaseType(NETMSGTYPE_RemoveNode)
      {
         this->nodeNumID = nodeNumID;

         this->nodeType = (int16_t)nodeType;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->nodeType
            % obj->nodeNumID;

         obj->serializeAckID(ctx);
      }


   protected:
      RemoveNodeMsg() : BaseType(NETMSGTYPE_RemoveNode)
      {
      }

   private:
      NumNodeID nodeNumID;
      int16_t nodeType; // NODETYPE_...

   public:
      NumNodeID getNodeNumID() const { return nodeNumID; }

      NodeType getNodeType() const { return (NodeType)nodeType; }
};


#endif /* REMOVENODEMSG_H_ */
