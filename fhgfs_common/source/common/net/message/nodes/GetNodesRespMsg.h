#ifndef GETNODESRESPMSG_H_
#define GETNODESRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/Common.h>


class GetNodesRespMsg : public NetMessageSerdes<GetNodesRespMsg>
{
   public:

      /**
       * @param rootID just a reference, so do not free it as long as you use this object!
       * @param rootIsBuddyMirrored
       * @param nodeList just a reference, so do not free it as long as you use this object!
       */
      GetNodesRespMsg(NumNodeID rootNumID, bool rootIsBuddyMirrored,
            std::vector<NodeHandle>& nodeList) :
         BaseType(NETMSGTYPE_GetNodesResp)
      {
         this->rootNumID = rootNumID;
         this->rootIsBuddyMirrored = rootIsBuddyMirrored;
         this->nodeList = &nodeList;
      }

      /**
       * For deserialization only
       */
      GetNodesRespMsg() : BaseType(NETMSGTYPE_GetNodesResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % serdes::backedPtr(obj->nodeList, obj->parsed.nodeList)
            % obj->rootNumID
            % obj->rootIsBuddyMirrored;
      }

   private:
      NumNodeID rootNumID;
      bool rootIsBuddyMirrored;

      // for serialization
      std::vector<NodeHandle>* nodeList; // not owned by this object!

      // for deserialization
      struct {
         std::vector<NodeHandle> nodeList;
      } parsed;

   public:
      std::vector<NodeHandle>& getNodeList()
      {
         return *nodeList;
      }

      NumNodeID getRootNumID() const
      {
         return rootNumID;
      }

      bool getRootIsBuddyMirrored()
      {
         return rootIsBuddyMirrored;
      }
};

#endif /*GETNODESRESPMSG_H_*/
